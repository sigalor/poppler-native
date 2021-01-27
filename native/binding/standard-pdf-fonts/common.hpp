#pragma once

#include <map>
#include <string>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "../build-charmap/TTFFile.hpp"

namespace StandardPDFFonts {

extern std::map<std::string, std::shared_ptr<BuildCharmap::TTFFile>> referenceFontStorage;

void initializeReferenceFontStorage(FT_Library library);
std::shared_ptr<BuildCharmap::TTFFile> getReferenceFont(std::string fontName);


}
