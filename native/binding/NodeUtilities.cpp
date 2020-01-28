#include "NodeUtilities.hpp"

namespace NodeUtilities {

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

}
