#include "PDFUtilities.hpp"

#include <goo/GooString.h>
#include <poppler/Dict.h>
#include <poppler/ErrorCodes.h>
#include <poppler/PDFDocEncoding.h>
#include <poppler/UnicodeMap.h>
#include <poppler/UnicodeMapFuncs.h>

namespace PDFUtilities {

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

std::map<std::string, std::string> getDictStrings(Dict* d, const std::vector<std::string>& keys, bool ignoreEmpty) {
  std::map<std::string, std::string> ret;
  for (auto& k : keys) {
    std::string curr = codepointsToUtf8(getDictString(d, k.c_str()));
    if (!ignoreEmpty || !curr.empty()) ret[k] = curr;
  }
  return ret;
}

std::string popplerErrorCodeToString(int errCode) {
  switch (errCode) {
    case errNone:
      return "no error";
    case errOpenFile:
      return "couldn't open the PDF file";
    case errBadCatalog:
      return "couldn't read the page catalog";
    case errDamaged:
      return "PDF file was damaged and couldn't be repaired";
    case errEncrypted:
      return "file was encrypted and password was incorrect or not supplied";
    case errHighlightFile:
      return "nonexistent or invalid highlight file";
    case errBadPrinter:
      return "invalid printer";
    case errPrinting:
      return "error during printing";
    case errPermission:
      return "PDF file doesn't allow that operation";
    case errBadPageNum:
      return "invalid page number";
    case errFileIO:
      return "file I/O error";
    case errFileChangedSinceOpen:
      return "file has changed since opening and save can't be done";
    default:
      return "unknown";
  }
}

}  // namespace PDFUtilities
