#include "StringMapper.hpp"

#include "../PDFUtilities.hpp"

namespace BuildCharmap {

StringMapper::StringMapper(FT_Library library, const std::string& inputFilename, std::shared_ptr<TTFFile> newReference) {
  input = std::make_shared<TTFFile>(library, inputFilename);
  reference = newReference;
}

std::string StringMapper::mapString(const std::string& str) {
  std::string ret;
  std::vector<Unicode> codepoints;

  for(unsigned char c : str) {
    if(input->glyphIndexExists((FT_UInt) c)) {
      FT_ULong codepoint = reference->findCodepoint(input->glyphFromIndex((FT_UInt) c));
      if(codepoint != 0)
        codepoints.push_back(codepoint);
    }
  }

  return PDFUtilities::codepointsToUtf8(codepoints);
}

}
