#pragma once

#include <string>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "TTFFile.hpp"

namespace BuildCharmap {

class StringMapper {
private:
  std::shared_ptr<TTFFile> input, reference;

public:
  StringMapper(FT_Library library, const std::string& inputFilename, std::shared_ptr<TTFFile> newReference);

  std::string mapString(const std::string& str);
};

}
