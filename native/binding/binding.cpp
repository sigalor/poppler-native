#include <goo/GooString.h>
#include <napi.h>
#include <poppler/ErrorCodes.h>
#include <poppler/GlobalParams.h>
#include <poppler/PDFDoc.h>
#include <poppler/Stream.h>
#include <poppler/TextOutputDev.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "NodeUtilities.hpp"
#include "PDFUtilities.hpp"
#include "html/HtmlOutputDev.h"
#include "text-repair/GlyphFingerprint.hpp"
#include "text-repair/TextRepair.hpp"

// Errors travel as exceptions with their message (std::runtime_error). In Microsoft's standard library without
// exceptions (_HAS_EXCEPTIONS=0, Node's default), an exception keeps only a pointer to its message, which then
// dangles: native/common.gypi turns them on for every target, and this keeps it that way.
#if defined(_MSC_VER) && !_HAS_EXCEPTIONS
#error "poppler-native must be compiled with _HAS_EXCEPTIONS=1 (see native/common.gypi)"
#endif

// Reads a whole file. The name arrives from JavaScript as UTF-8, which Windows needs converted to open it.
static std::vector<uint8_t> readFile(const std::string &filename) {
#ifdef _WIN32
  const std::filesystem::path path(std::u8string(filename.begin(), filename.end()));
  std::FILE *file = _wfopen(path.c_str(), L"rb");
#else
  std::FILE *file = std::fopen(filename.c_str(), "rb");
#endif
  if (!file) throw std::runtime_error(filename + ": couldn't open the PDF file: " + std::strerror(errno));
  std::vector<uint8_t> bytes;
  uint8_t chunk[65536];
  size_t n;
  while ((n = std::fread(chunk, 1, sizeof(chunk), file)) > 0) bytes.insert(bytes.end(), chunk, chunk + n);
  const bool failed = std::ferror(file);
  const int readError = errno;
  std::fclose(file);
  if (failed) throw std::runtime_error(filename + ": couldn't open the PDF file: " + std::strerror(readError));
  return bytes;
}

// What JavaScript asked for. index.js has validated the values; anything missing keeps its default.
struct Options {
  std::optional<std::string> password;
  int firstPage = 1;
  int lastPage = 0;  // 0: up to the last page
  bool images = true;
  bool layout = false;
  // The longest string JavaScript can hold (buffer.constants.MAX_STRING_LENGTH), passed by index.js: a result that
  // needs a longer one can't be returned.
  size_t maxStringLength = (size_t(1) << 29) - 24;
  // How many characters of data URLs one document may return, so that they fit in the JavaScript heap; index.js
  // passes a quarter of the heap's limit.
  size_t maxImageBytes = size_t(1) << 30;
};

// A count from JavaScript as a size_t: anything from 1 up, Infinity and numbers too large for size_t meaning "as many
// as fit", nothing for NaN or less than 1.
static std::optional<size_t> toSize(const Napi::Value &value) {
  if (!value.IsNumber()) return std::nullopt;
  const double number = value.As<Napi::Number>().DoubleValue();
  if (!(number >= 1)) return std::nullopt;
  if (number >= static_cast<double>(std::numeric_limits<size_t>::max())) return std::numeric_limits<size_t>::max();
  return static_cast<size_t>(number);
}

// What info() and text() share: checking the arguments, getting the PDF's bytes and opening the document.
class DocumentWorker : public Napi::AsyncWorker {
 public:
  DocumentWorker(const Napi::CallbackInfo &info, Napi::Promise::Deferred deferred)
      : Napi::AsyncWorker(info.Env()), deferred(deferred) {
    if (info.Length() < 1 || info.Length() > 2) {
      argError = "wrong number of arguments: expected 1-2, got " + std::to_string(info.Length());
      return;
    }
    if (!info[0].IsString() && !info[0].IsTypedArray()) {
      argError = "argument at position 1 has wrong type: expected string or Buffer, got " +
                 NodeUtilities::typeToString(info[0].Type());
      return;
    }
    if (info.Length() == 2) {
      if (!info[1].IsObject()) {
        argError = "argument at position 2 has wrong type: expected object, got " +
                   NodeUtilities::typeToString(info[1].Type());
        return;
      }
      readOptions(info[1].As<Napi::Object>());
    }

    if (info[0].IsString()) {
      fromFile = true;
      filename = info[0].ToString();  // read in Execute, off the JavaScript thread
    } else {
      // a Buffer is copied right away: the caller may reuse it while we work
      Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
      inputBytes = std::vector<uint8_t>(buf.Data(), buf.Data() + buf.Length());
      if (inputBytes.empty()) argError = "input buffer is empty";
    }
  }

 protected:
  std::unique_ptr<PDFDoc> openDocument() {
    // argument errors surface here, so that they reject the promise like every other error
    if (!argError.empty()) throw std::runtime_error(argError);
    if (fromFile) {
      inputBytes = readFile(filename);
      if (inputBytes.empty()) throw std::runtime_error("input buffer is empty");
    }

    std::optional<GooString> password;
    if (options.password) password.emplace(*options.password);
    auto doc = std::make_unique<PDFDoc>(
        std::make_unique<MemStream>((const char *)inputBytes.data(), 0, inputBytes.size(), Object::null()), password,
        password);
    if (!doc->isOk()) {
      const int errCode = doc->getErrorCode();
      std::string msg(PDFUtilities::popplerErrorCodeToString(errCode));
      if (errCode == errOpenFile) msg += ": " + std::string(std::strerror(doc->getFopenErrno()));
      throw std::runtime_error(msg);
    }
    return doc;
  }

  // The pages to read, clamped to the document; nothing when firstPage is past its end.
  std::optional<std::pair<int, int>> pageRange(PDFDoc &doc) const {
    const int numPages = doc.getNumPages();
    const int first = std::max(1, options.firstPage);
    const int last = options.lastPage > 0 ? std::min(options.lastPage, numPages) : numPages;
    if (first > last) return std::nullopt;
    return std::make_pair(first, last);
  }

  void OnError(const Napi::Error &error) override { deferred.Reject(error.Value()); }

  // Builds the result on the JavaScript thread. Should that fail (a string longer than JavaScript allows, memory), the
  // promise is rejected with the reason instead of the error escaping from the completion callback.
  template <typename Build>
  void Settle(Build build) {
    try {
      deferred.Resolve(build());
    } catch (const Napi::Error &e) {
      deferred.Reject(e.Value());
    } catch (const std::exception &e) {
      deferred.Reject(Napi::Error::New(Env(), e.what()).Value());
    }
  }

  Options options;
  Napi::Promise::Deferred deferred;

 private:
  void readOptions(const Napi::Object &object) {
    Napi::Value value;
    if ((value = object.Get("password")).IsString()) options.password = value.As<Napi::String>().Utf8Value();
    if ((value = object.Get("firstPage")).IsNumber()) options.firstPage = value.As<Napi::Number>().Int32Value();
    if ((value = object.Get("lastPage")).IsNumber()) options.lastPage = value.As<Napi::Number>().Int32Value();
    if ((value = object.Get("images")).IsBoolean()) options.images = value.As<Napi::Boolean>().Value();
    if ((value = object.Get("layout")).IsBoolean()) options.layout = value.As<Napi::Boolean>().Value();
    if (auto length = toSize(object.Get("_maxStringLength"))) options.maxStringLength = *length;
    if (auto bytes = toSize(object.Get("_maxImageBytes"))) options.maxImageBytes = *bytes;
  }

  std::string argError;
  bool fromFile = false;
  std::string filename;
  std::vector<uint8_t> inputBytes;
};

class InfoWorker : public DocumentWorker {
 public:
  using DocumentWorker::DocumentWorker;

  void Execute() override {
    std::unique_ptr<PDFDoc> doc = openDocument();
    numPages = doc->getNumPages();

    Object docInfo = doc->getDocInfo();
    if (docInfo.isDict()) {
      meta = PDFUtilities::getDictStrings(docInfo.getDict(), {"Title", "Author", "Keywords", "Subject", "Creator",
                                                              "Producer", "ModDate", "CreationDate"});
    }

    TextRepair::TextRepair textRepair(doc->getXRef());
    HtmlOutputDev outputDev(doc->getCatalog(), "this", numPages, !options.images, false, false, 0.1, pages,
                            &textRepair);
    outputDev.setMaxStringLength(options.maxStringLength);
    outputDev.setMaxImageBytes(options.maxImageBytes);
    if (auto range = pageRange(*doc)) {
      // stop at the first page after the result is known to be too large to return
      auto failed = [](void *dev) { return !static_cast<HtmlOutputDev *>(dev)->failure().empty(); };
      doc->displayPages(&outputDev, range->first, range->second, 108.0, 108.0, 0, true, false, false, failed, &outputDev);
    }
    if (!outputDev.failure().empty()) throw std::runtime_error(outputDev.failure());
    outputDev.generateOutline(doc.get(), outline);
  }

  // output has to be constructed here, because during Execute, all JS calls are invalid
  void OnOK() override {
    Settle([this] {
      Napi::Object ret = Napi::Object::New(Env());
      ret.Set("meta", NodeUtilities::serializeMap(Env(), meta));
      if (!outline.empty()) ret.Set("outline", ReadPDFOutputs::serializeArray(Env(), outline));

      std::vector<ReadPDFOutputs::Font> fonts;
      for (ReadPDFOutputs::Page &page : pages) fonts.insert(fonts.end(), page.fonts.begin(), page.fonts.end());
      ret.Set("fonts", ReadPDFOutputs::serializeArray(Env(), fonts));

      ret.Set("numPages", numPages);
      ret.Set("pages", ReadPDFOutputs::serializeArray(Env(), pages));
      return ret;
    });
  }

 private:
  int numPages = 0;
  std::map<std::string, std::string> meta;
  std::vector<ReadPDFOutputs::OutlineItem> outline;
  std::vector<ReadPDFOutputs::Page> pages;
};

// Where text() writes the text: up to the longest string JavaScript holds, counted as it grows. What doesn't fit is
// dropped and the rest of the document skipped, so memory stays bounded by that length.
struct TextSink {
  std::string text;
  size_t utf16Length = 0;  // what a JavaScript string's length counts
  size_t maxLength;
  bool full = false;

  void append(const char *data, int length) {
    if (full) return;
    size_t units = 0;
    for (int i = 0; i < length; ++i) {
      const auto byte = static_cast<unsigned char>(data[i]);
      if ((byte & 0xC0) != 0x80) units += byte >= 0xF0 ? 2 : 1;  // a lead byte; 4-byte sequences need a pair
    }
    if (units > maxLength - utf16Length) {
      full = true;
      return;
    }
    utf16Length += units;
    text.append(data, length);
  }
};

// Poppler's pdftotext engine, reading text drawn with an embedded Arial subset the same way info() does.
class RepairingTextOutputDev : public TextOutputDev {
 public:
  RepairingTextOutputDev(TextSink &out, bool physLayout, TextRepair::TextRepair &repair)
      : TextOutputDev(&append, &out, physLayout, 0, false), repair(repair) {}

  void drawChar(GfxState *state, double x, double y, double dx, double dy, double originX, double originY,
                CharCode code, int nBytes, const Unicode *u, int uLen) override {
    if (repair.repair(state, code, u, uLen, repaired)) {
      TextOutputDev::drawChar(state, x, y, dx, dy, originX, originY, code, nBytes, repaired.data(),
                              static_cast<int>(repaired.size()));
    } else {
      TextOutputDev::drawChar(state, x, y, dx, dy, originX, originY, code, nBytes, u, uLen);
    }
  }

 private:
  static void append(void *out, const char *text, int length) { static_cast<TextSink *>(out)->append(text, length); }

  TextRepair::TextRepair &repair;
  std::vector<Unicode> repaired;
};

class TextWorker : public DocumentWorker {
 public:
  using DocumentWorker::DocumentWorker;

  void Execute() override {
    std::unique_ptr<PDFDoc> doc = openDocument();
    TextRepair::TextRepair textRepair(doc->getXRef());
    sink.maxLength = options.maxStringLength;
    RepairingTextOutputDev textOut(sink, options.layout, textRepair);
    if (!textOut.isOk()) throw std::runtime_error("couldn't set up text extraction");
    if (auto range = pageRange(*doc)) {
      auto full = [](void *out) { return static_cast<TextSink *>(out)->full; };
      doc->displayPages(&textOut, range->first, range->second, 72.0, 72.0, 0, true, false, false, full, &sink);
    }
    if (sink.full) {
      throw std::runtime_error(
          "the text is too long for a JavaScript string; read fewer pages at a time with firstPage and lastPage");
    }
  }

  void OnOK() override {
    Settle([this] { return Napi::String::New(Env(), sink.text); });
  }

 private:
  TextSink sink;
};

template <typename Worker>
Napi::Value Run(const Napi::CallbackInfo &info) {
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(info.Env());
  (new Worker(info, deferred))->Queue();
  return deferred.Promise();
}

// Maintainer tool behind scripts/generate-reference-fingerprints.mjs: the outline fingerprints of a reference font,
// as [[fingerprint as 16 hex digits, [codepoints...]], ...]. Not part of the public API.
Napi::Value ReferenceFingerprints(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  if (info.Length() != 1 || !info[0].IsBuffer()) {
    throw Napi::TypeError::New(env, "_referenceFingerprints expects one Buffer holding a TrueType font");
  }
  Napi::Buffer<uint8_t> font = info[0].As<Napi::Buffer<uint8_t>>();

  FT_Library library;
  if (FT_Init_FreeType(&library) != 0) throw Napi::Error::New(env, "FT_Init_FreeType failed");
  std::vector<TextRepair::ReferenceEntry> entries;
  try {
    entries = TextRepair::referenceFingerprints(library, font.Data(), font.Length());
  } catch (const std::exception &e) {
    FT_Done_FreeType(library);
    throw Napi::Error::New(env, e.what());
  }
  FT_Done_FreeType(library);

  Napi::Array result = Napi::Array::New(env, entries.size());
  for (size_t i = 0; i < entries.size(); ++i) {
    char hex[17];
    snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(entries[i].fingerprint));
    Napi::Array codepoints = Napi::Array::New(env, entries[i].codepoints.size());
    for (size_t j = 0; j < entries[i].codepoints.size(); ++j) codepoints.Set(j, entries[i].codepoints[j]);
    Napi::Array entry = Napi::Array::New(env, 2);
    entry.Set(0u, Napi::String::New(env, hex));
    entry.Set(1u, codepoints);
    result.Set(i, entry);
  }
  return result;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  // Poppler's global settings, created once per process: the addon may be loaded by several worker threads, and
  // parsing threads read the settings concurrently.
  static std::once_flag initialized;
  std::call_once(initialized, [] {
    globalParams = std::make_unique<GlobalParams>();
    globalParams->setTextEncoding("UTF-8");
    // A library must not write to the user's stderr: Poppler's warnings about damaged PDFs are dropped, and real
    // failures reach the caller as rejected promises.
    globalParams->setErrQuiet(true);
  });

  exports.Set("info", Napi::Function::New(env, Run<InfoWorker>));
  exports.Set("text", Napi::Function::New(env, Run<TextWorker>));
  exports.Set("_referenceFingerprints", Napi::Function::New(env, ReferenceFingerprints));
  return exports;
}

NODE_API_MODULE(addon, Init);
