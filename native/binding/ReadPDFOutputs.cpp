#include "ReadPDFOutputs.hpp"

namespace ReadPDFOutputs {

Napi::Object Font::serialize(Napi::Env& env) const {
  Napi::Object ret = Napi::Object::New(env);
  ret.Set("id", id);
  ret.Set("size", size);
  ret.Set("family", Napi::String::New(env, family));
  ret.Set("color", Napi::String::New(env, color));
  ret.Set("bold", bold);
  ret.Set("italic", italic);
  return ret;
}

Napi::Object Image::serialize(Napi::Env& env) const {
  Napi::Object ret = Napi::Object::New(env);
  ret.Set("top", top);
  ret.Set("left", left);
  ret.Set("width", width);
  ret.Set("height", height);
  ret.Set("src", Napi::String::New(env, src));
  return ret;
}

Napi::Object String::serialize(Napi::Env& env) const {
  Napi::Object ret = Napi::Object::New(env);
  ret.Set("top", top);
  ret.Set("left", left);
  ret.Set("width", width);
  ret.Set("height", height);
  ret.Set("font", font);
  ret.Set("text", Napi::String::New(env, text));
  if (!link.empty()) ret.Set("link", Napi::String::New(env, link));
  return ret;
}

Napi::Object Page::serialize(Napi::Env& env) const {
  Napi::Object ret = Napi::Object::New(env);
  ret.Set("number", number);
  ret.Set("width", width);
  ret.Set("height", height);
  ret.Set("fonts", serializeArray(env, fonts));
  ret.Set("images", serializeArray(env, images));
  ret.Set("strings", serializeArray(env, strings));
  return ret;
}

Napi::Object OutlineItem::serialize(Napi::Env& env) const {
  Napi::Object ret = Napi::Object::New(env);
  ret.Set("page", page);
  ret.Set("title", Napi::String::New(env, title));
  if (!children.empty()) ret.Set("children", serializeArray(env, children));
  return ret;
}

}  // namespace ReadPDFOutputs
