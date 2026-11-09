#pragma once

#include <vector>

#include <png.h>

// Encodes an image as PNG into memory, row by row, with exactly the settings of Poppler's goo/PNGWriter (best
// compression, 72 DPI, RGB-8 or 1-bit gray), so its output is byte-identical to what poppler-native always produced.
// Portable: no FILE* in between. libpng reports errors by longjmp, so every call into libpng happens in a frame
// without C++ objects to unwind, and its messages never reach stderr.
class PngEncoder {
 public:
  enum class Format { Rgb, Monochrome };

  PngEncoder(int width, int height, Format format);
  ~PngEncoder();
  PngEncoder(const PngEncoder &) = delete;
  PngEncoder &operator=(const PngEncoder &) = delete;

  bool ok() const { return !failed_; }
  // One row: 3 bytes per pixel for Rgb, 8 pixels per byte for Monochrome.
  bool writeRow(const unsigned char *row);
  // Ends the image and hands over the PNG bytes; empty if anything failed.
  std::vector<unsigned char> finish();

 private:
  static void onWrite(png_structp png, png_bytep bytes, png_size_t length);
  static void onFlush(png_structp png);
  static void onError(png_structp png, png_const_charp message);
  static void onWarning(png_structp png, png_const_charp message);

  png_structp png_ = nullptr;
  png_infop info_ = nullptr;
  std::vector<unsigned char> data_;
  bool failed_ = false;
};
