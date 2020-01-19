#pragma once

#include <string>

#include <napi.h>

void throwJS(const Napi::Env &env, const std::string &msg) {
  Napi::TypeError::New(env, msg).ThrowAsJavaScriptException();
}
