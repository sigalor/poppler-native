#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <poppler/CharTypes.h>
#include <poppler/Object.h>

class GfxFont;
class GfxState;
class XRef;

namespace TextRepair {

class ReferenceFont;

// Reads text drawn with an embedded subset of a reference font (see ReferenceFonts.hpp) whose PDF font has no
// ToUnicode map, by the shape of the glyph actually drawn: the glyph Poppler's own renderer would draw for the code
// is fingerprinted and looked up in the reference font. Poppler's text stands whenever the shape agrees with it
// (a correctly decoded Cyrillic A keeps its codepoint, though Latin A has the same outline) or is unknown.
// One instance per document and thread.
class TextRepair {
 public:
  explicit TextRepair(XRef *xref);
  ~TextRepair();
  TextRepair(const TextRepair &) = delete;
  TextRepair &operator=(const TextRepair &) = delete;

  // True if the glyph drawn for `code` contradicts the text Poppler decoded for it (u, uLen); `out` then holds the
  // text the glyph's shape stands for.
  bool repair(GfxState *state, CharCode code, const Unicode *u, int uLen, std::vector<Unicode> &out);

 private:
  struct Font {
    const ReferenceFont *reference = nullptr;  // nullptr: not a font the repair can read
    std::vector<unsigned char> program;        // FreeType reads the face from this buffer
    FT_Face face = nullptr;
    std::vector<int> codeToGID;  // empty: the code is the glyph index
    std::unordered_map<FT_UInt, std::optional<uint64_t>> fingerprints;
  };

  Font &fontFor(GfxFont &font);
  void load(GfxFont &gfxFont, Font &font);

  XRef *xref_;
  FT_Library library_ = nullptr;
  std::map<Ref, Font> fonts_;
};

}  // namespace TextRepair
