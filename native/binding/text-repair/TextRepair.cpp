#include "TextRepair.hpp"

#include <algorithm>
#include <span>

#include <fofi/FoFiTrueType.h>
#include <poppler/GfxFont.h>
#include <poppler/GfxState.h>

#include "GlyphFingerprint.hpp"
#include "ReferenceFonts.hpp"

namespace TextRepair {

TextRepair::TextRepair(XRef *xref) : xref_(xref) {
  if (FT_Init_FreeType(&library_) != 0) library_ = nullptr;
}

TextRepair::~TextRepair() {
  for (auto &[ref, font] : fonts_) {
    if (font.face) FT_Done_Face(font.face);
  }
  if (library_) FT_Done_FreeType(library_);
}

TextRepair::Font &TextRepair::fontFor(GfxFont &gfxFont) {
  auto [it, inserted] = fonts_.try_emplace(*gfxFont.getID());
  if (inserted) load(gfxFont, it->second);
  return it->second;
}

// Mirrors SplashOutputDev::doUpdateFont for embedded TrueType fonts, so the glyph fingerprinted is the one Poppler
// draws for a code.
void TextRepair::load(GfxFont &gfxFont, Font &font) {
  if (!library_ || gfxFont.hasToUnicodeCMap() || !gfxFont.getName()) return;
  const ReferenceFont *reference = referenceFontFor(*gfxFont.getName());
  if (!reference) return;
  const GfxFontType type = gfxFont.getType();
  const bool simple = type == fontTrueType || type == fontTrueTypeOT;
  const bool cid = type == fontCIDType2 || type == fontCIDType2OT;
  if (!simple && !cid) return;

  std::optional<std::vector<unsigned char>> program = gfxFont.readEmbFontFile(xref_);
  if (!program || program->empty()) return;
  font.program = std::move(*program);

  if (simple) {
    std::unique_ptr<FoFiTrueType> ff = FoFiTrueType::make(font.program, 0);
    if (!ff) return;
    font.codeToGID = static_cast<Gfx8BitFont &>(gfxFont).getCodeToGIDMap(ff.get());
  } else {
    GfxCIDFont &cidFont = static_cast<GfxCIDFont &>(gfxFont);
    if (cidFont.getCIDToGIDLen() > 0) {
      font.codeToGID = cidFont.getCIDToGID();
    } else {
      std::unique_ptr<FoFiTrueType> ff = FoFiTrueType::make(font.program, 0);
      if (!ff) return;
      font.codeToGID = cidFont.getCodeToGIDMap(ff.get());
    }
  }

  if (FT_New_Memory_Face(library_, font.program.data(), static_cast<FT_Long>(font.program.size()), 0, &font.face)) {
    font.face = nullptr;
    return;
  }
  font.reference = reference;
}

bool TextRepair::repair(GfxState *state, CharCode code, const Unicode *u, int uLen, std::vector<Unicode> &out) {
  GfxFont *gfxFont = state->getFont().get();
  if (!gfxFont) return false;
  Font &font = fontFor(*gfxFont);
  if (!font.reference) return false;

  // as SplashFTFont::makeGlyph: a code beyond the map is the glyph index itself
  const long gid = code < font.codeToGID.size() ? font.codeToGID[code] : static_cast<long>(code);
  if (gid <= 0 || gid >= font.face->num_glyphs) return false;  // 0 is .notdef, < 0 "do not draw"

  auto cached = font.fingerprints.find(static_cast<FT_UInt>(gid));
  if (cached == font.fingerprints.end()) {
    cached = font.fingerprints.emplace(static_cast<FT_UInt>(gid), glyphFingerprint(font.face, static_cast<FT_UInt>(gid))).first;
  }
  if (!cached->second) return false;

  std::span<const uint32_t> codepoints = font.reference->lookup(*cached->second);
  if (codepoints.empty()) return false;
  if (uLen == 1 && std::find(codepoints.begin(), codepoints.end(), u[0]) != codepoints.end()) return false;
  out.assign(1, codepoints[0]);
  return true;
}

}  // namespace TextRepair
