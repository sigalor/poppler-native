#include <memory>
#include <string>

#include <goo/GooString.h>
#include <napi.h>
#include <poppler/GlobalParams.h>
#include <poppler/PDFDocFactory.h>

#include "html/HtmlOutputDev.h"
#include "node-utils.hpp"
#include "pdf-utils.hpp"

Napi::Object GetPDFInfo(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  Napi::Object ret = Napi::Object::New(env);
  if (info.Length() < 1 || !info[0].IsString()) {
    throwJS(env, "expected filename as first parameter");
    return ret;
  }

  // load document (TODO: do not print error messages to stdout/stderr here)
  globalParams = std::make_unique<GlobalParams>();
  char textEncoding[] = "UTF-8";
  globalParams->setTextEncoding(textEncoding);
  GooString filenameGoo(info[0].As<Napi::String>());
  PDFDoc *doc = PDFDocFactory().createPDFDoc(filenameGoo, nullptr, nullptr);
  if (!doc->isOk()) {
    delete doc;
    throwJS(env, "failed to load PDF document");
    return ret;
  }

  // get document metadata
  Object docInfo = doc->getDocInfo();
  if (docInfo.isDict()) {
    ret.Set("meta",
            serializeMap(env, getDictStrings(docInfo.getDict(), {"Title", "Author", "Keywords", "Subject", "Creator",
                                                                 "Producer", "ModDate", "CreationDate"})));
  }

  std::vector<ReadPDFOutputs::Page> pages;
  std::vector<ReadPDFOutputs::OutlineItem> outline;
  HtmlOutputDev *outputDev =
      new HtmlOutputDev(doc->getCatalog(), "this", doc->getNumPages(), false, false, false, 0.1, pages);
  doc->displayPages(outputDev, 1, doc->getNumPages(), 108.0, 108.0, 0, true, false, false);
  outputDev->generateOutline(doc, outline);

  if (!outline.empty()) ret.Set("outline", ReadPDFOutputs::serializeArray(env, outline));
  ret.Set("pages", ReadPDFOutputs::serializeArray(env, pages));
  delete outputDev;

  return ret;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("info", Napi::Function::New(env, GetPDFInfo));
  return exports;
}

NODE_API_MODULE(addon, Init);
