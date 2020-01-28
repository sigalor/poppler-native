#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include <goo/GooString.h>
#include <napi.h>
#include <poppler/ErrorCodes.h>
#include <poppler/GlobalParams.h>
#include <poppler/PDFDoc.h>
#include <poppler/Stream.h>

#include "NodeUtilities.hpp"
#include "PDFUtilities.hpp"
#include "html/HtmlOutputDev.h"

class ReadPDFWorker : public Napi::AsyncWorker {
 private:
  std::string argError;
  std::string filename;
  std::vector<uint8_t> inputBytes;
  bool useFilename = false;

  std::map<std::string, std::string> meta;
  std::vector<ReadPDFOutputs::OutlineItem> outline;
  std::vector<ReadPDFOutputs::Page> pages;
  Napi::Promise::Deferred deferred;

 public:
  ReadPDFWorker(Napi::Env &env, const Napi::CallbackInfo &info, Napi::Promise::Deferred &newDeferred)
      : Napi::AsyncWorker(env), deferred(newDeferred) {
    if (info.Length() != 1) {
      argError = "wrong number of arguments: expected 1, got " + std::to_string(info.Length());
      return;
    } else if (!info[0].IsString() && !info[0].IsTypedArray()) {
      argError = "argument at position 1 has wrong type: expected string or Buffer, got " +
                 NodeUtilities::typeToString(info[0].Type());
      return;
    }

    if (info[0].IsString()) {
      filename = info[0].As<Napi::String>();
      useFilename = true;
    } else {
      Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
      inputBytes = std::vector<uint8_t>(buf.Data(), buf.Data() + buf.Length());
      if (inputBytes.empty()) argError = "input buffer is empty";
    }
  }

  void Execute() {
    // handle function argument errors here to catch exceptions correctly
    if (!argError.empty()) throw std::runtime_error(argError);

    // load PDF document
    PDFDoc *doc;
    MemStream *memStream = nullptr;
    if (useFilename) {
      GooString *filenameGoo = new GooString(filename);  // will be deleted internally by PDFDoc
      doc = new PDFDoc(filenameGoo);
    } else {
      memStream = new MemStream((const char *)inputBytes.data(), 0, inputBytes.size(), Object(objNull));
      doc = new PDFDoc(memStream);
    }

    if (!doc->isOk()) {
      int errCode = doc->getErrorCode(), fopenErrno = doc->getFopenErrno();
      delete doc;

      std::string msg;
      if (!filename.empty()) msg += filename + ": ";
      msg += PDFUtilities::popplerErrorCodeToString(errCode);
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
    HtmlOutputDev *outputDev =
        new HtmlOutputDev(doc->getCatalog(), "this", doc->getNumPages(), false, false, false, 0.1, pages);
    doc->displayPages(outputDev, 1, doc->getNumPages(), 108.0, 108.0, 0, true, false, false);
    outputDev->generateOutline(doc, outline);
    delete outputDev;
    delete doc;
  }

  // output has to be constructed here, because during Execute, all JS calls are invalid
  void OnOK() {
    Napi::Object ret = Napi::Object::New(Env());
    ret.Set("meta", NodeUtilities::serializeMap(Env(), meta));
    if (!outline.empty()) ret.Set("outline", ReadPDFOutputs::serializeArray(Env(), outline));
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
    char textEncoding[] = "UTF-8";
    globalParams->setTextEncoding(textEncoding);
  }

  ReadPDFWorker *readPDFWorker = new ReadPDFWorker(env, info, deferred);
  readPDFWorker->Queue();
  return deferred.Promise();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("info", Napi::Function::New(env, GetPDFInfo));
  return exports;
}

NODE_API_MODULE(addon, Init);
