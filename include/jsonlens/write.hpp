/*
 * write.hpp - Turn the jsonlens value model into a JSON text.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * to_json_text() is the way out for a program that builds a document with the
 * API and has to hand JSON to something else - a socket, a file, a log. It
 * closes the gap that the model alone leaves open: value.hpp can build and
 * inspect a document without the backend, but only the backend can write it.
 *
 * The text is the backend's text. The document is copied into a backend tree and
 * printed by the pinned backend, so every producer that links this library writes
 * the same dialect, checked by the same golden tests: insertion order, TAB
 * indentation when formatted, one line for an array, the backend's number format,
 * and a null for a value that is not a number. A second printer in this library
 * would be a second dialect, so there is none.
 *
 * What the model can hold and JSON cannot is refused rather than lost: a string
 * that carries a NUL byte cannot be written, because the backend, like JSON
 * itself, ends a string at a C string terminator. An integer is exact up to
 * 2^53, beyond which the backend holds a double, and a number that is not a
 * number is written as null, which is what the golden tests pin.
 *
 * This header exposes the JSON backend.
 */
#ifndef JSONLENS_WRITE_HPP
#define JSONLENS_WRITE_HPP

#include <cstddef>
#include <string>

#include "jsonlens/detail/cjson.hpp"
#include "jsonlens/detail/cjson_owner.hpp"
#include "jsonlens/json_index.hpp"
#include "jsonlens/value.hpp"

namespace jsonlens {

namespace detail {

/* Copy one value of the model into a fresh node of the backend. Returns the null
   pointer and fills error when the value cannot be written, and the path says
   which one. The caller owns the node. */
inline cJSON *to_cjson(const JsonValue &value, const std::string &path, std::string &error) {
  const std::string where = path.empty() ? std::string("the document") : "'" + path + "'";
  switch (value.kind()) {
  case JsonKind::null_value:
    return cJSON_CreateNull();
  case JsonKind::boolean: {
    bool flag = false;
    value.as_bool(flag);
    return cJSON_CreateBool(flag ? 1 : 0);
  }
  case JsonKind::integer: {
    long long number = 0;
    value.as_integer(number);
    return cJSON_CreateNumber(static_cast<double>(number));
  }
  case JsonKind::number: {
    double number = 0.0;
    value.as_number(number);
    return cJSON_CreateNumber(number);
  }
  case JsonKind::string: {
    std::string text;
    value.as_string(text);
    if (text.find('\0') != std::string::npos) {
      error = "the string at " + where + " holds a NUL byte, which the backend cannot write";
      return 0;
    }
    return cJSON_CreateString(text.c_str());
  }
  case JsonKind::array: {
    CJsonTree array(cJSON_CreateArray());
    if (array.empty()) {
      error = "the JSON backend ran out of memory";
      return 0;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
      const JsonValue *const item = value.at(index);
      cJSON *const node = item == 0 ? cJSON_CreateNull()
                                    : to_cjson(*item, append_json_path_index(path, index), error);
      if (node == 0) {
        return 0;
      }
      cJSON_AddItemToArray(array.get(), node);
    }
    return array.release();
  }
  case JsonKind::object: {
    CJsonTree object(cJSON_CreateObject());
    if (object.empty()) {
      error = "the JSON backend ran out of memory";
      return 0;
    }
    const std::vector<std::string> keys = value.keys();
    for (std::size_t index = 0; index < keys.size(); ++index) {
      /* A key is a C string to the backend just as a value is, and it is copied
         before the member is written, so a NUL here would truncate the key and
         not the member. */
      if (keys[index].find('\0') != std::string::npos) {
        error = "the key at " + where + " holds a NUL byte, which the backend cannot write";
        return 0;
      }
      const JsonValue *const item = value.member(keys[index]);
      cJSON *const node = item == 0
                              ? cJSON_CreateNull()
                              : to_cjson(*item, append_json_path_key(path, keys[index]), error);
      if (node == 0) {
        return 0;
      }
      cJSON_AddItemToObject(object.get(), keys[index].c_str(), node);
    }
    return object.release();
  }
  }
  return cJSON_CreateNull();
}

} /* namespace detail */

/* Write a document as JSON. Returns false and fills error when the value cannot
   be written, and then leaves text untouched. formatted selects the TAB indented
   layout of the backend; both layouts are byte stable and covered by the golden
   tests. */
inline bool to_json_text(const JsonValue &value, std::string &text, std::string &error,
                         bool formatted = true) {
  detail::CJsonTree tree(detail::to_cjson(value, std::string(), error));
  if (tree.empty()) {
    return false;
  }
  detail::CJsonText printed(formatted ? cJSON_Print(tree.get())
                                      : cJSON_PrintUnformatted(tree.get()));
  if (printed.empty()) {
    error = "the JSON backend ran out of memory";
    return false;
  }
  text.assign(printed.get());
  return true;
}

/* The same write for a caller that has nothing to do with a failure. An empty
   string means the document could not be written, which is unambiguous because
   a document that was written is never empty - a null document is the four
   characters "null". */
inline std::string to_json_text(const JsonValue &value, bool formatted = true) {
  std::string text;
  std::string error;
  if (!to_json_text(value, text, error, formatted)) {
    return std::string();
  }
  return text;
}

} /* namespace jsonlens */

#endif /* JSONLENS_WRITE_HPP */
