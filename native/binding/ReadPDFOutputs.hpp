#pragma once

#include <string>
#include <vector>

#include <napi.h>

namespace ReadPDFOutputs {

class Font {
 public:
  int id;
  unsigned int size;
  std::string family, color;
  bool bold, italic;
  Napi::Object serialize(Napi::Env env) const;
};

class Image {
 public:
  double top, left, width, height;
  std::string src;
  Napi::Object serialize(Napi::Env env) const;
};

class String {
 public:
  double top, left, width, height;
  int font;
  std::string text, link;
  Napi::Object serialize(Napi::Env env) const;
};

class Page {
 public:
  int number, width, height;
  std::vector<Font> fonts;
  std::vector<Image> images;
  std::vector<String> strings;
  Napi::Object serialize(Napi::Env env) const;
};

class OutlineItem {
 public:
  int page;
  std::string title;
  std::vector<OutlineItem> children;
  Napi::Object serialize(Napi::Env env) const;
};

template <typename T>
Napi::Array serializeArray(Napi::Env env, const std::vector<T>& items) {
  Napi::Array ret = Napi::Array::New(env, items.size());
  for (size_t i = 0; i < items.size(); ++i) ret.Set(i, items[i].serialize(env));
  return ret;
}

}  // namespace ReadPDFOutputs
