#include <ft2build.h>
#include <glob.h>
#include <goo/GooString.h>
#include <napi.h>
#include <poppler/ErrorCodes.h>
#include <poppler/GlobalParams.h>
#include <poppler/PDFDoc.h>
#include <poppler/Stream.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include FT_FREETYPE_H

#include "NodeUtilities.hpp"
#include "PDFUtilities.hpp"
#include "build-charmap/StringMapper.hpp"
#include "html/HtmlOutputDev.h"
#include "standard-pdf-fonts/common.hpp"

FT_Library freeTypeLibrary;

class ReadPDFWorker : public Napi::AsyncWorker {
 private:
  std::string argError;
  std::vector<uint8_t> inputBytes;
  bool reconvertThroughPS;

  std::map<std::string, std::string> meta;
  std::vector<ReadPDFOutputs::OutlineItem> outline;
  std::vector<ReadPDFOutputs::Page> pages;
  Napi::Promise::Deferred deferred;

 public:
  ReadPDFWorker(Napi::Env &env, const Napi::CallbackInfo &info, Napi::Promise::Deferred &newDeferred)
      : Napi::AsyncWorker(env), deferred(newDeferred) {
    reconvertThroughPS = false;
    if (info.Length() < 1 || info.Length() > 2) {
      argError = "wrong number of arguments: expected 1-2, got " + std::to_string(info.Length());
      return;
    }
    if (!info[0].IsString() && !info[0].IsTypedArray()) {
      argError = "argument at position 1 has wrong type: expected string or Buffer, got " +
                 NodeUtilities::typeToString(info[0].Type());
      return;
    }
    if (info.Length() == 2) {
      if (!info[1].IsObject()) {
        argError = "argument at position 2 has wrong type: expected object, got " +
                   NodeUtilities::typeToString(info[1].Type());
        return;
      }
      Napi::Object obj = info[1].ToObject();
      if (obj.Has("reconvertThroughPS") && obj.Get("reconvertThroughPS").IsBoolean()) {
        reconvertThroughPS = obj.Get("reconvertThroughPS").ToBoolean();
      }
    }

    if (info[0].IsString()) {
      std::string filename(info[0].ToString());
      std::ifstream inStream(filename, std::ios::in | std::ios::binary);
      if (!inStream) {
        argError = filename + ": couldn't open the PDF file: " + std::strerror(errno);
        return;
      }
      inputBytes = std::vector<uint8_t>((std::istreambuf_iterator<char>(inStream)), std::istreambuf_iterator<char>());
    } else {
      Napi::Buffer<uint8_t> buf = info[0].As<Napi::Buffer<uint8_t>>();
      inputBytes = std::vector<uint8_t>(buf.Data(), buf.Data() + buf.Length());
    }
    if (inputBytes.empty()) argError = "input buffer is empty";
  }

  void ExecCMD(std::string cmd) {
    FILE *command = popen(cmd.c_str(), "r");
    if (command == nullptr) {
      throw std::runtime_error("failed to execute: " + cmd);
    }

    std::string output;
    char line[1000];
    while (fgets(line, sizeof(line), command) != nullptr) {
      output += std::string(line);
    }

    // in case of an error, throw it
    int pcloseCode = pclose(command);
    int code = WEXITSTATUS(pcloseCode);
    if (code != 0) {
      throw std::runtime_error("failed to execute '" + cmd + "': code " + std::to_string(code) + ": " + output);
    }
  }

  void Execute() {
    // handle function argument errors here to catch exceptions correctly
    if (!argError.empty()) throw std::runtime_error(argError);

    char tempTemplate[] = "/tmp/poppler-native.tmp.XXXXXX";

    if (reconvertThroughPS) {
      char tempDirPath[256];
      strncpy(tempDirPath, tempTemplate, sizeof(tempDirPath));
      if (mkdtemp(tempDirPath) == nullptr)
        throw std::runtime_error(std::string("failed to create temporary directory: ") + std::strerror(errno));

      // create temporary file for dumping the PDF there
      // char tempFilePath[512];              // array to hold the result.
      // strcpy(tempFilePath, tempDirPath);   // copy string one into the result.
      // strcat(tempFilePath, "/input.pdf");  // append string two to the result.
      char tempInputPDFPath[256];
      strncpy(tempInputPDFPath, tempTemplate, sizeof(tempInputPDFPath));
      if (mkstemp(tempInputPDFPath) == -1)
        throw std::runtime_error(std::string("failed to create temporary filename: ") + std::strerror(errno));
      FILE *tmpFile = fopen(tempInputPDFPath, "wb");
      fwrite(inputBytes.data(), 1, inputBytes.size(), tmpFile);
      fclose(tmpFile);

      // convert PDF to .ps
      char tempPsFilePath[256];
      strncpy(tempPsFilePath, tempTemplate, sizeof(tempPsFilePath));
      if (mkstemp(tempPsFilePath) == -1)
        throw std::runtime_error(std::string("failed to create temporary filename: ") + std::strerror(errno));
      ExecCMD(std::string("bash -c 'cd ") + tempDirPath + "; gs -o " + tempPsFilePath +
              " -dNOCACHE -sDEVICE=ps2write " + tempInputPDFPath + " 2>&1'");

      // reconvert .ps to PDF
      char tempNewPDFPath[256];
      strncpy(tempNewPDFPath, tempTemplate, sizeof(tempNewPDFPath));
      if (mkstemp(tempNewPDFPath) == -1)
        throw std::runtime_error(std::string("failed to create temporary filename: ") + std::strerror(errno));
      ExecCMD(std::string("bash -c 'cd ") + tempDirPath + "; gs -o " + tempNewPDFPath + " -sDEVICE=pdfwrite " +
              tempPsFilePath + " 2>&1'");

      std::string filename(tempNewPDFPath);
      std::ifstream inStream(filename, std::ios::in | std::ios::binary);
      if (!inStream) {
        argError = filename + ": couldn't open the PDF file: " + std::strerror(errno);
        return;
      }
      inputBytes = std::vector<uint8_t>((std::istreambuf_iterator<char>(inStream)), std::istreambuf_iterator<char>());

      unlink(tempInputPDFPath);
      unlink(tempPsFilePath);
      unlink(tempNewPDFPath);

      int code = system((std::string("rm -rf ") + tempDirPath).c_str());
      if (code != 0) throw std::runtime_error("failed to remove temporary directory: code " + std::to_string(code));
    }

    // load PDF document from memory
    MemStream *memStream = new MemStream((const char *)inputBytes.data(), 0, inputBytes.size(), Object(objNull));
    PDFDoc *doc = new PDFDoc(memStream);

    if (!doc->isOk()) {
      int errCode = doc->getErrorCode(), fopenErrno = doc->getFopenErrno();
      delete doc;

      std::string msg(PDFUtilities::popplerErrorCodeToString(errCode));
      if (errCode == errOpenFile) msg += ": " + std::string(std::strerror(fopenErrno));
      throw std::runtime_error(msg);
    }

    // read meta data
    Object docInfo = doc->getDocInfo();
    if (docInfo.isDict()) {
      meta = PDFUtilities::getDictStrings(docInfo.getDict(), {"Title", "Author", "Keywords", "Subject", "Creator",
                                                              "Producer", "ModDate", "CreationDate"});
    }

    // parse entire document and generate outline
    HtmlOutputDev *outputDev =
        new HtmlOutputDev(doc->getCatalog(), "this", doc->getNumPages(), false, false, false, 0.1, pages);
    doc->displayPages(outputDev, 1, doc->getNumPages(), 108.0, 108.0, 0, true, false, false);
    outputDev->generateOutline(doc, outline);
    delete outputDev;
    delete doc;

    // fix fonts where hasToUnicodeCMap is false by reconstructing the character map
    char tempDirPath[256];
    memset(tempDirPath, 0, sizeof(tempDirPath));

    // maps from ReadPDFOutputs::Font::fullName to a mapper to reconstruct this embedded font
    std::map<std::string, std::shared_ptr<BuildCharmap::StringMapper>> stringMappers;

    // maps the IDs of bad fonts to their fullName properties
    std::map<int, std::string> badFonts;

    for (ReadPDFOutputs::Page &page : pages) {
      for (const ReadPDFOutputs::Font &font : page.fonts) {
        // if the first font that has no Unicode CMap is found, extract all fonts from that file

        if (!font.hasToUnicodeCMap) {
          badFonts.emplace(font.id, font.fullName);

          if (tempDirPath[0] == 0) {
            // create temporary directory for files generated by 'mutool extract'
            strncpy(tempDirPath, tempTemplate, sizeof(tempDirPath));
            if (mkdtemp(tempDirPath) == nullptr)
              throw std::runtime_error(std::string("failed to create temporary directory: ") + std::strerror(errno));

            // create temporary file for dumping the PDF there
            char tempFilePath[256];
            strncpy(tempFilePath, tempTemplate, sizeof(tempFilePath));
            if (mkstemp(tempFilePath) == -1)
              throw std::runtime_error(std::string("failed to create temporary filename: ") + std::strerror(errno));
            FILE *tmpFile = fopen(tempFilePath, "wb");
            fwrite(inputBytes.data(), 1, inputBytes.size(), tmpFile);
            fclose(tmpFile);

            // execute mutool extract
            ExecCMD(std::string("bash -c 'cd ") + tempDirPath + "; mutool extract " + tempFilePath + " 2>&1'");

            unlink(tempFilePath);
          }

          if (stringMappers.find(font.fullName) == stringMappers.end()) {
            std::shared_ptr<BuildCharmap::TTFFile> referenceFont = StandardPDFFonts::getReferenceFont(font.fullName);
            if (referenceFont != nullptr) {
              // mutool appends object ID to filename, thus the real filename needs to be found using glob
              std::string inputFilenameGlob = std::string(tempDirPath) + "/" + font.fullName + "-*.ttf";
              glob_t globResults;
              int globCode = glob(inputFilenameGlob.c_str(), 0, nullptr, &globResults);
              if (globCode != GLOB_NOMATCH) {
                if (globCode != 0) throw std::runtime_error(std::string("failed to run glob: ") + std::strerror(errno));

                if (globResults.gl_pathc >= 1) {
                  std::string inputFilename = globResults.gl_pathv[0];
                  stringMappers.emplace(font.fullName, std::make_shared<BuildCharmap::StringMapper>(
                                                           freeTypeLibrary, inputFilename, referenceFont));
                }
              }
              globfree(&globResults);
            }
          }
        }
      }

      for (ReadPDFOutputs::String &str : page.strings) {
        if (badFonts.find(str.font) != badFonts.end()) {
          if (stringMappers.find(badFonts[str.font]) != stringMappers.end()) {
            str.text = stringMappers.at(badFonts[str.font])->mapString(str.text);
          }
        }
      }
    }

    if (tempDirPath != nullptr) {
      int code = system((std::string("rm -rf ") + tempDirPath).c_str());
      if (code != 0) throw std::runtime_error("failed to remove temporary directory: code " + std::to_string(code));
    }
  }

  // output has to be constructed here, because during Execute, all JS calls are invalid
  void OnOK() {
    Napi::Object ret = Napi::Object::New(Env());
    ret.Set("meta", NodeUtilities::serializeMap(Env(), meta));
    if (!outline.empty()) ret.Set("outline", ReadPDFOutputs::serializeArray(Env(), outline));

    std::vector<ReadPDFOutputs::Font> fonts;
    for (ReadPDFOutputs::Page &page : pages) fonts.insert(fonts.end(), page.fonts.begin(), page.fonts.end());
    ret.Set("fonts", ReadPDFOutputs::serializeArray(Env(), fonts));

    ret.Set("pages", ReadPDFOutputs::serializeArray(Env(), pages));
    deferred.Resolve(ret);
  }

  void OnError(Napi::Error const &error) { deferred.Reject(error.Value()); }
};

Napi::Object GetPDFInfo(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);

  // globalParams (from Poppler) is so not thread-safe, so hopefully this works
  if (!globalParams) {
    globalParams = std::make_unique<GlobalParams>();
    char textEncoding[] = "UTF-8";
    globalParams->setTextEncoding(textEncoding);
  }

  ReadPDFWorker *readPDFWorker = new ReadPDFWorker(env, info, deferred);
  readPDFWorker->Queue();
  return deferred.Promise();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  FT_Init_FreeType(&freeTypeLibrary);
  StandardPDFFonts::initializeReferenceFontStorage(freeTypeLibrary);

  exports.Set("info", Napi::Function::New(env, GetPDFInfo));
  return exports;
}

NODE_API_MODULE(addon, Init);
