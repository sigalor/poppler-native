#include "common.hpp"

#include <algorithm>

#include "Arial.hpp"
#include "Arial_Bold.hpp"

namespace StandardPDFFonts {

std::map<std::string, std::shared_ptr<BuildCharmap::TTFFile>> referenceFontStorage;

void initializeReferenceFontStorage(FT_Library library) {
  referenceFontStorage.emplace("Arial", std::make_shared<BuildCharmap::TTFFile>(library, Arial, Arial_length, true));
  referenceFontStorage.emplace("Arial-Bold", std::make_shared<BuildCharmap::TTFFile>(library, Arial_Bold, Arial_Bold_length, true));
}

std::shared_ptr<BuildCharmap::TTFFile> getReferenceFont(std::string fontName) {
  // remove prefix for embedded PDF fonts (see https://tex.stackexchange.com/a/156438)
  if(fontName.size() >= 8 && fontName[6] == '+' && std::all_of(fontName.begin(), fontName.begin()+6, [](char c) { return c >= 'A' && c <= 'Z'; })) {
    fontName = fontName.substr(7);
  }

  // remove "MT" suffix (typically in ArialMT)
  if(fontName.size() >= 3 && fontName[fontName.size() - 2] == 'M' && fontName[fontName.size() - 1] == 'T') {
    fontName = fontName.substr(0, fontName.size() - 2);
  }
 
  if(referenceFontStorage.find(fontName) == referenceFontStorage.end()) {
    return nullptr;
  } else {
    return referenceFontStorage[fontName];
  }
}

}
