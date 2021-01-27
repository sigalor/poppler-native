#pragma once

#include <string>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace BuildCharmap {

void checkFreeTypeError(const std::string& description, FT_Error code);

}
