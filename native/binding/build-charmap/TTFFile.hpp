#pragma once

#include <string>
#include <map>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "Glyph.hpp"

namespace BuildCharmap {

class TTFFile {
private:
	FT_Library library;
	FT_Face face;
  std::map<FT_UInt, std::shared_ptr<Glyph>> glyphs;

  void loadAllGlyphs();

public:
  TTFFile();
	TTFFile(FT_Library newLibrary, const std::string& filename, bool loadAllGlyphs = false);
  TTFFile(FT_Library newLibrary, unsigned char* buffer, unsigned int size, bool loadAllGlyphs = false);
	~TTFFile();

	std::shared_ptr<Glyph> glyphFromIndex(FT_UInt glyphIndex);
	bool glyphIndexExists(FT_UInt glyphIndex) const;
  FT_ULong findCodepoint(std::shared_ptr<Glyph> glyph) const;
};

}
