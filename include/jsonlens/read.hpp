/*
 * read.hpp - Turn a JSON text into the jsonlens value model.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * parse_json_text() is the way in for a program that is handed JSON - a
 * configuration, a message, a report - and wants to look at it with the C++ API
 * instead of the C API of the backend. It is the same read that the configuration
 * loader performs, on the same two authorities: the strict scanner of
 * json_index.hpp decides whether the text is JSON and where every token is, and
 * the pinned backend decides what every node holds.
 *
 * The read is stricter than a JSON parser, because a lenient parser is how a
 * typo becomes a silent default:
 *
 *   - the whole text must be one JSON value, so trailing data is an error;
 *   - an object key that is written more than once is an error, where a parser
 *     drops all but the last;
 *   - the text must stay within json_nesting_limit() levels, which is stricter
 *     than the depth the backend itself accepts, so that the backend never has to
 *     decide whether a document this library read is too deep;
 *   - a number written as an integer stays an integer in the model, and a number
 *     written with a fraction or an exponent becomes a double, which is the only
 *     place that distinction survives, because the backend holds both as a
 *     double.
 *
 * The model is the same JsonValue that a configuration element hands out, so a
 * value read here can be compared, copied and written with the rest of the C++
 * API. An empty path means the whole document; paths use the syntax of the
 * diagnostics, "device.port" or "pads[2].name".
 *
 * This header exposes the JSON backend.
 */
#ifndef JSONLENS_READ_HPP
#define JSONLENS_READ_HPP

#include <string>

#include "jsonlens/detail/cjson.hpp"
#include "jsonlens/json_index.hpp"
#include "jsonlens/value.hpp"

namespace jsonlens {

namespace detail {

/* One value of a document, copied from the backend tree into the model. The
   strict scanner decides whether a number was written as an integer, which is
   the only place that distinction survives: the backend holds both as a double.
   The path is the position of the value, and it is what makes the difference
   between "1" and "1.0" visible here. */
inline JsonValue to_json_value(const JsonIndex &index, const std::string &path, const cJSON *item) {
  if (item == 0 || cJSON_IsNull(item)) {
    return JsonValue::make_null();
  }
  if (cJSON_IsString(item)) {
    return JsonValue::make_string(item->valuestring != 0 ? item->valuestring : std::string());
  }
  if (cJSON_IsBool(item)) {
    return JsonValue::make_bool(cJSON_IsTrue(item) != 0);
  }
  if (cJSON_IsNumber(item)) {
    if (index.kind(path) == JsonTokenKind::integer) {
      return JsonValue::make_integer(static_cast<long long>(item->valuedouble));
    }
    return JsonValue::make_number(item->valuedouble);
  }
  if (cJSON_IsArray(item)) {
    JsonValue array = JsonValue::make_array();
    std::size_t position = 0;
    for (const cJSON *child = item->child; child != 0; child = child->next) {
      array.add(to_json_value(index, append_json_path_index(path, position), child));
      ++position;
    }
    return array;
  }
  if (cJSON_IsObject(item)) {
    JsonValue object = JsonValue::make_object();
    for (const cJSON *child = item->child; child != 0; child = child->next) {
      const std::string key = child->string != 0 ? child->string : std::string();
      object.add(key, to_json_value(index, append_json_path_key(path, key), child));
    }
    return object;
  }
  return JsonValue::make_null();
}

} /* namespace detail */

/* Read one JSON document. Returns false and fills error when the text is not
   JSON, when a key repeats or when the backend refuses it. The model is left
   untouched on failure, and any value is accepted as a document, including a
   number or a string at the top level. */
inline bool parse_json_text(const std::string &text, JsonValue &value, std::string &error) {
  JsonIndex index;
  std::string scan_error;
  if (!JsonIndex::build(text, index, scan_error)) {
    error = "not valid JSON: " + scan_error;
    return false;
  }
  const std::string duplicated = index.duplicated_key_list();
  if (!duplicated.empty()) {
    error = "duplicated object key(s): " + duplicated;
    return false;
  }
  cJSON *const root = cJSON_Parse(text.c_str());
  if (root == 0) {
    error = "not valid JSON: the JSON backend rejected the document";
    return false;
  }
  value = detail::to_json_value(index, std::string(), root);
  cJSON_Delete(root);
  return true;
}

} /* namespace jsonlens */

#endif /* JSONLENS_READ_HPP */
