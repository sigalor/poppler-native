#include <goo/GooString.h>
#include <napi.h>
#include <poppler/ErrorCodes.h>
#include <poppler/GlobalParams.h>
#include <poppler/PDFDoc.h>
#include <poppler/Stream.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "NodeUtilities.hpp"
#include "PDFUtilities.hpp"
#include "html/HtmlOutputDev.h"
#include "text-repair/GlyphFingerprint.hpp"
#include "text-repair/TextRepair.hpp"

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

class ReadPDFWorker : public Napi::AsyncWorker {
 private:
  std::string argError;
  bool fromFile = false;
  std::string filename;             // read in Execute, off the JavaScript thread
  std::vector<uint8_t> inputBytes;  // a Buffer is copied right away: the caller may reuse it while we work

  std::map<std::string, std::string> meta;
  std::vector<ReadPDFOutputs::OutlineItem> outline;
  std::vector<ReadPDFOutputs::Page> pages;
  Napi::Promise::Deferred deferred;

 public:
  ReadPDFWorker(Napi::Env &env, const Napi::CallbackInfo &info, Napi::Promise::Deferred &newDeferred)
      : Napi::AsyncWorker(env), deferred(newDeferred) {
    if (info.Length() < 1 || info.Length() > 2) {
      argError = "wrong number of arguments: expected 1-2, got " + std::to_string(info.Length());
      return;
    }
    if (!info[0].IsString() && !info[0].IsTypedArray()) {
      argError = "argument at position 1 has wrong type: expected string or Buffer, got " +
                 NodeUtilities::typeToString(info[0].Type());
      return;
    }
    if (info.Length() == 2 && !info[1].IsObject()) {
      argError = "argument at position 2 has wrong type: expected object, got " +
                 NodeUtilities::typeToString(info[1].Type());
      return;
    }

    if (info[0].IsString()) {
      fromFile = true;
      filename = info[0].ToString();
    } else {
      Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
      inputBytes = std::vector<uint8_t>(buf.Data(), buf.Data() + buf.Length());
      if (inputBytes.empty()) argError = "input buffer is empty";
    }
  }

  void Execute() {
    // handle function argument errors here to catch exceptions correctly
    if (!argError.empty()) throw std::runtime_error(argError);
    if (fromFile) {
      inputBytes = readFile(filename);
      if (inputBytes.empty()) throw std::runtime_error("input buffer is empty");
    }

    // load PDF document from memory
    auto doc = std::make_unique<PDFDoc>(
        std::make_unique<MemStream>((const char *)inputBytes.data(), 0, inputBytes.size(), Object::null()));

    if (!doc->isOk()) {
      int errCode = doc->getErrorCode(), fopenErrno = doc->getFopenErrno();

      std::string msg(PDFUtilities::popplerErrorCodeToString(errCode));
      if (errCode == errOpenFile) msg += ": " + std::string(std::strerror(fopenErrno));
      throw std::runtime_error(msg);
    }

    // read meta data
    Object docInfo = doc->getDocInfo();
    if (docInfo.isDict()) {
      meta = PDFUtilities::getDictStrings(docInfo.getDict(), {"Title", "Author", "Keywords", "Subject", "Creator",
                                                              "Producer", "ModDate", "CreationDate"});
    }

    // parse entire document and generate outline
    {
      TextRepair::TextRepair textRepair(doc->getXRef());
      HtmlOutputDev outputDev(doc->getCatalog(), "this", doc->getNumPages(), false, false, false, 0.1, pages,
                              &textRepair);
      doc->displayPages(&outputDev, 1, doc->getNumPages(), 108.0, 108.0, 0, true, false, false);
      outputDev.generateOutline(doc.get(), outline);
    }
  }

  // output has to be constructed here, because during Execute, all JS calls are invalid
  void OnOK() {
    Napi::Object ret = Napi::Object::New(Env());
    ret.Set("meta", NodeUtilities::serializeMap(Env(), meta));
    if (!outline.empty()) ret.Set("outline", ReadPDFOutputs::serializeArray(Env(), outline));

    std::vector<ReadPDFOutputs::Font> fonts;
    for (ReadPDFOutputs::Page &page : pages) fonts.insert(fonts.end(), page.fonts.begin(), page.fonts.end());
    ret.Set("fonts", ReadPDFOutputs::serializeArray(Env(), fonts));

    ret.Set("pages", ReadPDFOutputs::serializeArray(Env(), pages));
    deferred.Resolve(ret);
  }

  void OnError(Napi::Error const &error) { deferred.Reject(error.Value()); }
};

Napi::Object GetPDFInfo(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  // globalParams (from Poppler) is so not thread-safe, so hopefully this works
  if (!globalParams) {
    globalParams = std::make_unique<GlobalParams>();
    globalParams->setTextEncoding("UTF-8");
  }

  ReadPDFWorker *readPDFWorker = new ReadPDFWorker(env, info, deferred);
  readPDFWorker->Queue();
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
  exports.Set("info", Napi::Function::New(env, GetPDFInfo));
  exports.Set("_referenceFingerprints", Napi::Function::New(env, ReferenceFingerprints));
  return exports;
}

NODE_API_MODULE(addon, Init);
