#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <goo/GooString.h>
#include <napi.h>
#include <poppler/GfxState.h>
#include <poppler/GlobalParams.h>
#include <poppler/Link.h>
#include <poppler/OutputDev.h>
#include <poppler/PDFDocEncoding.h>
#include <poppler/PDFDocFactory.h>
#include <poppler/UnicodeMap.h>
#include <poppler/UnicodeMapFuncs.h>

std::string codepointsToUtf8(const std::vector<Unicode>& codepoints) {
  UnicodeMap uMap("UTF-8", true, &mapUTF8);
  std::string ret;
  char buf[8];
  int n;

  for (Unicode cp : codepoints) {
    if ((n = uMap.mapUnicode(cp, buf, sizeof(buf)))) ret += std::string(buf, buf + n);
  }

  return ret;
}

// From https://gitlab.freedesktop.org/poppler/poppler/blob/master/utils/pdftohtml.cc#L471
// but instead of encoding to HTML encoding, it returns a list of Unicode codepoints
std::vector<Unicode> getDictString(Dict* d, const char* key) {
  // Get raw value as read from PDF (may be in pdfDocEncoding or UCS2)
  Object obj = d->lookup(key);
  if (!obj.isString()) return {};
  const GooString* rawString = obj.getString();

  // Convert rawString to unicode, store if rawString is UCS2 (as opposed to pdfDocEncoding)
  bool isUnicode;
  int unicodeLength;
  if (rawString->hasUnicodeMarker()) {
    isUnicode = true;
    unicodeLength = (obj.getString()->getLength() - 2) / 2;
  } else {
    isUnicode = false;
    unicodeLength = obj.getString()->getLength();
  }

  // Read all Unicode characters one-by-one and encode them to UTF-8
  std::vector<Unicode> ret;
  for (int i = 0; i < unicodeLength; i++) {
    if (isUnicode) {
      ret.push_back(((rawString->getChar((i + 1) * 2) & 0xff) << 8) | (rawString->getChar(((i + 1) * 2) + 1) & 0xff));
    } else {
      ret.push_back(pdfDocEncoding[rawString->getChar(i) & 0xff]);
    }
  }

  return ret;
}

std::map<std::string, std::string> getDictStrings(Dict* d, const std::vector<std::string>& keys) {
  std::map<std::string, std::string> ret;
  for (auto& k : keys) ret[k] = codepointsToUtf8(getDictString(d, k.c_str()));
  return ret;
}
