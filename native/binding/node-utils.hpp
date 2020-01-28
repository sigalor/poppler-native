#pragma once

#include <string>
#include <type_traits>

#include <napi.h>

std::string typeToString(napi_valuetype type) {
  switch (type) {
    case napi_undefined:
      return "undefined";
    case napi_null:
      return "null";
    case napi_boolean:
      return "boolean";
    case napi_number:
      return "number";
    case napi_string:
      return "string";
    case napi_symbol:
      return "symbol";
    case napi_object:
      return "object";
    case napi_function:
      return "function";
    case napi_external:
      return "external";
    case napi_bigint:
      return "bigint";
    default:
      return "unknown";
  }
}

template <typename T>
Napi::Object serializeMap(Napi::Env env, const std::map<std::string, T> &data) {
  Napi::Object ret = Napi::Object::New(env);
  for (auto &i : data) {
    if (std::is_same<T, std::string>::value)
      ret.Set(i.first, Napi::String::New(env, i.second));
    else
      ret.Set(i.first, i.second);
  }
  return ret;
}
