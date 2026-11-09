#include "PngEncoder.hpp"

#include <csetjmp>

namespace {
constexpr double kDpi = 72;  // as HtmlOutputDev always asked goo/PNGWriter for
}

PngEncoder::PngEncoder(int width, int height, Format format) {
  png_ = png_create_write_struct(PNG_LIBPNG_VER_STRING, this, &PngEncoder::onError, &PngEncoder::onWarning);
  if (!png_) {
    failed_ = true;
    return;
  }
  info_ = png_create_info_struct(png_);
  if (!info_ || setjmp(png_jmpbuf(png_))) {
    failed_ = true;
    return;
  }
  png_set_write_fn(png_, this, &PngEncoder::onWrite, &PngEncoder::onFlush);
  png_set_compression_level(png_, 9);  // Z_BEST_COMPRESSION
  const bool rgb = format == Format::Rgb;
  png_set_IHDR(png_, info_, width, height, rgb ? 8 : 1, rgb ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_GRAY,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  const png_uint_32 pixelsPerMeter = static_cast<png_uint_32>(kDpi / 0.0254);
  png_set_pHYs(png_, info_, pixelsPerMeter, pixelsPerMeter, PNG_RESOLUTION_METER);
  png_write_info(png_, info_);
}

PngEncoder::~PngEncoder() {
  if (png_) png_destroy_write_struct(&png_, info_ ? &info_ : nullptr);
}

bool PngEncoder::writeRow(const unsigned char *row) {
  if (failed_) return false;
  if (setjmp(png_jmpbuf(png_))) {
    failed_ = true;
    return false;
  }
  png_write_row(png_, const_cast<png_bytep>(row));
  return true;
}

std::vector<unsigned char> PngEncoder::finish() {
  if (failed_) return {};
  if (setjmp(png_jmpbuf(png_))) {
    failed_ = true;
    return {};
  }
  png_write_end(png_, info_);
  return std::move(data_);
}

void PngEncoder::onWrite(png_structp png, png_bytep bytes, png_size_t length) {
  auto *self = static_cast<PngEncoder *>(png_get_io_ptr(png));
  bool appended = true;
  try {
    self->data_.insert(self->data_.end(), bytes, bytes + length);
  } catch (...) {
    appended = false;
  }
  if (!appended) png_error(png, "out of memory");
}

void PngEncoder::onFlush(png_structp) {}

void PngEncoder::onError(png_structp png, png_const_charp) {
  png_longjmp(png, 1);
}

void PngEncoder::onWarning(png_structp, png_const_charp) {}
