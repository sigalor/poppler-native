#pragma once

#include <string>
#include <type_traits>

#include <napi.h>

void throwJS(Napi::Env &env, const std::string &msg) { Napi::TypeError::New(env, msg).ThrowAsJavaScriptException(); }

template <typename T>
Napi::Object serializeMap(Napi::Env &env, const std::map<std::string, T> &data) {
  Napi::Object ret = Napi::Object::New(env);
  for (auto &i : data) {
    if (std::is_same<T, std::string>::value)
      ret.Set(i.first, Napi::String::New(env, i.second));
    else
      ret.Set(i.first, i.second);
  }
  return ret;
}
