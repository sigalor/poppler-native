#pragma once

#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace BuildCharmap {

class Glyph {
private:
	FT_GlyphSlot slot;
	FT_Outline ftoutline;
	FT_Glyph_Metrics gm;
	FT_Face face;

public:
  FT_UInt glyphIndex;
	FT_ULong codepoint;

	std::vector<FT_Vector> points;
	std::vector<char> tags;
	std::vector<short> contours;

	int bbwidth, bbheight;

	Glyph(FT_Face face, FT_UInt glyphIndex);
	Glyph(FT_Face face, FT_UInt newGlyphIndex, FT_ULong newCodepoint);

	bool operator==(const Glyph& other) const;
};

}
