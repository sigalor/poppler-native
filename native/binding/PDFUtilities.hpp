#pragma once

#include <map>
#include <string>
#include <vector>

#include <poppler/CharTypes.h>
#include <poppler/Object.h>

#include <poppler/Dict.h>

namespace PDFUtilities {

std::string codepointsToUtf8(const std::vector<Unicode>& codepoints);
std::vector<Unicode> getDictString(Dict* d, const char* key);
std::map<std::string, std::string> getDictStrings(Dict* d, const std::vector<std::string>& keys,
                                                  bool ignoreEmpty = true);

}  // namespace PDFUtilities
