#include "Glyph.hpp"

#include <sstream>
#include "common.hpp"

namespace BuildCharmap {

Glyph::Glyph(FT_Face face, FT_UInt glyphIndex) : Glyph(face, glyphIndex, 0) {}

Glyph::Glyph(FT_Face face, FT_UInt newGlyphIndex, FT_ULong newCodepoint) : glyphIndex(newGlyphIndex), codepoint(newCodepoint) {
  checkFreeTypeError("FT_Load_Glyph", FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_SCALE));
  slot = face->glyph;
  ftoutline = slot->outline;
  gm = slot->metrics;

	points = std::vector<FT_Vector>(ftoutline.points, ftoutline.points + ftoutline.n_points);
	tags = std::vector<char>(ftoutline.tags, ftoutline.tags + ftoutline.n_points);
	contours = std::vector<short>(ftoutline.contours, ftoutline.contours + ftoutline.n_contours);
	bbheight = face->bbox.yMax - face->bbox.yMin;
  bbwidth = face->bbox.xMax - face->bbox.xMin;
}

bool Glyph::operator==(const Glyph& other) const {
  if (points.size() != other.points.size())
    return false;
  if (contours.size() != other.contours.size())
    return false;

  for (size_t i = 0; i < points.size(); i++) {
    if (points[i].x != other.points[i].x || points[i].y != other.points[i].y)
      return false;
    if (tags[i] != other.tags[i])
      return false;
  }

  for (size_t i = 0; i < contours.size(); i++) {
    if (contours[i] != other.contours[i])
      return false;
  }

  return true;
}

}
