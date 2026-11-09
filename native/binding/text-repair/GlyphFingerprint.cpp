#include "GlyphFingerprint.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace TextRepair {

namespace {

class Fnv1a64 {
 public:
  // Feeds the value as 8 little-endian bytes, so the hash is the same on every host.
  void add(int64_t value) {
    const uint64_t bits = static_cast<uint64_t>(value);
    for (int byte = 0; byte < 8; ++byte) {
      hash_ ^= (bits >> (8 * byte)) & 0xff;
      hash_ *= 0x100000001b3ULL;
    }
  }
  uint64_t value() const { return hash_; }

 private:
  uint64_t hash_ = 0xcbf29ce484222325ULL;
};

}  // namespace

std::optional<uint64_t> glyphFingerprint(FT_Face face, FT_UInt glyphIndex) {
  if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_SCALE) != 0) return std::nullopt;
  const FT_Outline &outline = face->glyph->outline;
  Fnv1a64 hash;
  hash.add(outline.n_contours);
  hash.add(outline.n_points);
  for (int i = 0; i < outline.n_contours; ++i) hash.add(outline.contours[i]);
  for (int i = 0; i < outline.n_points; ++i) {
    hash.add(outline.points[i].x);
    hash.add(outline.points[i].y);
    hash.add(outline.tags[i] & 3);  // on-curve, conic or cubic; the other bits are FreeType bookkeeping
  }
  return hash.value();
}

std::vector<ReferenceEntry> referenceFingerprints(FT_Library library, const unsigned char *data, size_t size) {
  FT_Face face;
  if (FT_Error code = FT_New_Memory_Face(library, data, static_cast<FT_Long>(size), 0, &face)) {
    throw std::runtime_error("FT_New_Memory_Face: code " + std::to_string(code));
  }
  std::unique_ptr<FT_FaceRec, decltype(&FT_Done_Face)> faceGuard(face, &FT_Done_Face);

  // glyph index -> its codepoints, both ascending (the cmap is walked in codepoint order)
  std::map<FT_UInt, std::vector<uint32_t>> codepointsOfGlyph;
  FT_UInt glyphIndex;
  for (FT_ULong codepoint = FT_Get_First_Char(face, &glyphIndex); glyphIndex != 0;
       codepoint = FT_Get_Next_Char(face, codepoint, &glyphIndex)) {
    codepointsOfGlyph[glyphIndex].push_back(static_cast<uint32_t>(codepoint));
  }

  std::vector<ReferenceEntry> entries;
  std::unordered_map<uint64_t, size_t> entryOf;
  for (const auto &[glyph, codepoints] : codepointsOfGlyph) {
    std::optional<uint64_t> fingerprint = glyphFingerprint(face, glyph);
    if (!fingerprint) continue;
    auto existing = entryOf.find(*fingerprint);
    if (existing == entryOf.end()) {
      entryOf.emplace(*fingerprint, entries.size());
      entries.push_back({*fingerprint, codepoints});
    } else {
      std::vector<uint32_t> &all = entries[existing->second].codepoints;
      all.insert(all.end(), codepoints.begin(), codepoints.end());
    }
  }
  for (ReferenceEntry &entry : entries) std::sort(entry.codepoints.begin() + 1, entry.codepoints.end());
  return entries;
}

}  // namespace TextRepair
