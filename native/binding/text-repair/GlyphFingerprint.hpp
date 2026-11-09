#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace TextRepair {

// A 64-bit FNV-1a hash of a glyph's unscaled outline: its contour and point counts, contour end points, point
// coordinates in font units and on/off-curve tags. Two glyphs with the same fingerprint have the same shape, which
// is how text drawn with an embedded subset of a known font is identified. std::nullopt if the glyph can't be loaded.
std::optional<uint64_t> glyphFingerprint(FT_Face face, FT_UInt glyphIndex);

struct ReferenceEntry {
  uint64_t fingerprint;
  // Every codepoint the font draws with this outline. The first is the one poppler-native 2.3.0's repair picked
  // for the outline (the lowest glyph index that has it, and that glyph's lowest codepoint); the rest ascend.
  std::vector<uint32_t> codepoints;
};

// One entry per distinct outline reachable from the font's Unicode cmap, in order of first glyph index.
// Throws std::runtime_error if FreeType can't open the font.
std::vector<ReferenceEntry> referenceFingerprints(FT_Library library, const unsigned char *data, size_t size);

}  // namespace TextRepair
