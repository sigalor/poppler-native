#include "TTFFile.hpp"

#include <stdexcept>
#include "common.hpp"

namespace BuildCharmap {

TTFFile::TTFFile() {}

TTFFile::TTFFile(FT_Library newLibrary, const std::string& filename, bool loadAllGlyphs) : library(newLibrary) {
  checkFreeTypeError("FT_New_Face", FT_New_Face(library, filename.c_str(), 0, &face));
  if(loadAllGlyphs)
    this->loadAllGlyphs();
}

TTFFile::TTFFile(FT_Library newLibrary, unsigned char* buffer, unsigned int size, bool loadAllGlyphs) : library(newLibrary) {
  checkFreeTypeError("FT_New_Memory_Face", FT_New_Memory_Face(library, buffer, size, 0, &face));
  if(loadAllGlyphs)
    this->loadAllGlyphs();
}

TTFFile::~TTFFile() {
  checkFreeTypeError("FT_Done_Face", FT_Done_Face(face));
}

std::shared_ptr<Glyph> TTFFile::glyphFromIndex(FT_UInt glyphIndex) {
  if(glyphs.find(glyphIndex) == glyphs.end())
    glyphs.emplace(glyphIndex, std::make_shared<Glyph>(face, glyphIndex));
  return glyphs.at(glyphIndex);
}

bool TTFFile::glyphIndexExists(FT_UInt glyphIndex) const {
  return FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_SCALE) == 0;
}

void TTFFile::loadAllGlyphs() {
  FT_UInt glyphIndex;
  FT_ULong codepoint = FT_Get_First_Char(face, &glyphIndex);

  if (glyphIndex == 0) {
    // if the first glyph index is 0 already, try to query for glyphs anyway
    for (FT_UInt i = 0; i < 0x10000; i++) {
      if(glyphIndexExists(i))
        glyphs.emplace(i, std::make_shared<Glyph>(face, i));
    }
  } else {
    while (glyphIndex != 0) {
      glyphs.emplace(glyphIndex, std::make_shared<Glyph>(face, glyphIndex, codepoint));
      codepoint = FT_Get_Next_Char(face, codepoint, &glyphIndex);
    }
  }
}

FT_ULong TTFFile::findCodepoint(std::shared_ptr<Glyph> glyph) const {
  for(auto refGlyph : glyphs) {
    if(*glyph == *refGlyph.second)
      return refGlyph.second->codepoint;
  }
  return 0;
}

}
