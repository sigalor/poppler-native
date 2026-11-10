#include "ReferenceFonts.hpp"

#include <algorithm>
#include <vector>

namespace TextRepair {

namespace {
#include "reference-fingerprints.inc"

const std::vector<ReferenceFont> &referenceFonts() {
  static const std::vector<ReferenceFont> fonts = [] {
    std::vector<ReferenceFont> result;
    for (const ReferenceFontData &data : kReferenceFontData) result.emplace_back(data);
    return result;
  }();
  return fonts;
}
}  // namespace

std::span<const uint32_t> ReferenceFont::lookup(uint64_t fingerprint) const {
  const uint64_t *end = data_.fingerprints + data_.count;
  const uint64_t *hit = std::lower_bound(data_.fingerprints, end, fingerprint);
  if (hit == end || *hit != fingerprint) return {};
  const size_t index = hit - data_.fingerprints;
  return {data_.codepoints + data_.offsets[index], data_.codepoints + data_.offsets[index + 1]};
}

const ReferenceFont *referenceFontFor(std::string_view name) {
  if (name.size() >= 8 && name[6] == '+' &&
      std::all_of(name.begin(), name.begin() + 6, [](char c) { return c >= 'A' && c <= 'Z'; })) {
    name.remove_prefix(7);
  }
  if (name.size() >= 3 && name.ends_with("MT")) name.remove_suffix(2);
  for (const ReferenceFont &font : referenceFonts()) {
    if (font.name() == name) return &font;
  }
  return nullptr;
}

}  // namespace TextRepair
