#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include <goo/GooString.h>
#include <napi.h>
#include <poppler/GlobalParams.h>
#include <poppler/PDFDocFactory.h>

#include "html/HtmlOutputDev.h"
#include "node-utils.hpp"
#include "pdf-utils.hpp"

class ReadPDFWorker : public Napi::AsyncWorker {
 private:
  std::string filename;
  std::map<std::string, std::string> meta;
  std::vector<ReadPDFOutputs::OutlineItem> outline;
  std::vector<ReadPDFOutputs::Page> pages;
  Napi::Promise::Deferred deferred;

 public:
  ReadPDFWorker(Napi::Env &env, const std::string &newFilename, Napi::Promise::Deferred &newDeferred)
      : Napi::AsyncWorker(env), filename(newFilename), deferred(newDeferred) {}

  void Execute() {
    // load PDF document (TODO: do not print error messages to stdout/stderr here)
    GooString filenameGoo(filename);
    PDFDoc *doc = PDFDocFactory().createPDFDoc(GooString(filename), nullptr, nullptr);
    if (!doc->isOk()) {
      delete doc;
      throw std::runtime_error("failed to load PDF document");
    }

    // read meta data
    Object docInfo = doc->getDocInfo();
    if (docInfo.isDict()) {
      meta = getDictStrings(docInfo.getDict(), {"Title", "Author", "Keywords", "Subject", "Creator", "Producer",
                                                "ModDate", "CreationDate"});
    }

    // parse entire document and generate outline
    HtmlOutputDev *outputDev =
        new HtmlOutputDev(doc->getCatalog(), "this", doc->getNumPages(), false, false, false, 0.1, pages);
    doc->displayPages(outputDev, 1, doc->getNumPages(), 108.0, 108.0, 0, true, false, false);
    outputDev->generateOutline(doc, outline);
    delete outputDev;
    delete doc;
  }

  void OnOK() {
    Napi::Object ret = Napi::Object::New(Env());
    ret.Set("meta", serializeMap(Env(), meta));
    if (!outline.empty()) ret.Set("outline", ReadPDFOutputs::serializeArray(Env(), outline));
    ret.Set("pages", ReadPDFOutputs::serializeArray(Env(), pages));
    deferred.Resolve(ret);
  }

  void OnError(Napi::Error const &error) { deferred.Reject(error.Value()); }
};

Napi::Object GetPDFInfo(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  Napi::Object ret = Napi::Object::New(env);
  if (info.Length() < 1 || !info[0].IsString()) {
    throwJS(env, "expected filename as first parameter");
    return ret;
  }

  std::string filename = info[0].As<Napi::String>();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  // globalParams is so not thread-safe, so hopefully this works
  if (!globalParams) {
    globalParams = std::make_unique<GlobalParams>();
    char textEncoding[] = "UTF-8";
    globalParams->setTextEncoding(textEncoding);
  }

  ReadPDFWorker *readPDFWorker = new ReadPDFWorker(env, filename, deferred);
  readPDFWorker->Queue();
  return deferred.Promise();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("info", Napi::Function::New(env, GetPDFInfo));
  return exports;
}

NODE_API_MODULE(addon, Init);
