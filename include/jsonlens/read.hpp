/*
 * read.hpp - Turn a JSON text into the jsonlens value model.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * parse_json_text() is the way in for a program that is handed JSON - a
 * configuration, a message, a report - and wants to look at it through this
 * library instead of through the C API of the backend. It reads with two
 * authorities: the strict scanner of json_index.hpp decides whether the text is
 * JSON and where every token sits, and the pinned backend decides what every
 * node holds.
 *
 * The read is stricter than a JSON parser, because a lenient parser is how a
 * typo becomes a silent default:
 *
 *   - the whole text must be one JSON value, so trailing data is an error;
 *   - an object key written twice is an error, where a parser drops all but the
 *     last;
 *   - the text must stay within json_nesting_limit() levels, which is stricter
 *     than the depth the backend itself accepts, so that the backend never has to
 *     decide whether a document read here is too deep;
 *   - a number written as an integer stays an integer in the model, and a number
 *     written with a fraction or an exponent becomes a double. That distinction
 *     survives only here, because the backend holds both as one double. An
 *     integer that a double cannot hold exactly, which is every integer at or
 *     above 2^53, stays a number rather than become a long long that is not the
 *     number that was written;
 *   - the text must be valid UTF-8, because it is read as text and written back
 *     as text, and a byte sequence that is not UTF-8 cannot make that trip;
 *   - the text must not contain a NUL byte, because the backend reads a C string
 *     and would see a shorter text than the scanner checked.
 *
 * The result is the JsonValue the rest of the API works with, so a value read
 * here can be inspected, copied and written again. Paths use the syntax of
 * json_index.hpp, "device.port" or "pads[2].name", and an empty path means the
 * whole document.
 *
 * This header exposes the JSON backend.
 */
#ifndef JSONLENS_READ_HPP
#define JSONLENS_READ_HPP

#include <cmath>
#include <string>

#include "jsonlens/decimal.hpp"
#include "jsonlens/detail/cjson.hpp"
#include "jsonlens/detail/cjson_owner.hpp"
#include "jsonlens/json_index.hpp"
#include "jsonlens/value.hpp"

namespace jsonlens {

namespace detail {

/* One value of a document, copied from the backend tree into the model. The
   strict scanner decides whether a number was written as an integer, which is
   the only place that distinction survives: the backend holds both as a double.
   The path is the position of the value, and it is what makes the difference
   between "1" and "1.0" visible here.

   False means the text is valid JSON but does not fit the model, and the only
   case of that is a number the backend cannot hold: an exponent large enough to
   overflow a double, which the backend turns into an infinity and would print as
   null. A silent 1e400 -> null is exactly the kind of loss this library refuses,
   so it is an error that names the path instead. */
inline bool to_json_value(const JsonIndex &index, const std::string &path, const cJSON *item,
                          JsonValue &value, std::string &error) {
  if (item == 0 || cJSON_IsNull(item)) {
    value = JsonValue::make_null();
    return true;
  }
  if (cJSON_IsString(item)) {
    value = JsonValue::make_string(item->valuestring != 0 ? item->valuestring : std::string());
    return true;
  }
  if (cJSON_IsBool(item)) {
    value = JsonValue::make_bool(cJSON_IsTrue(item) != 0);
    return true;
  }
  if (cJSON_IsNumber(item)) {
    const double number = item->valuedouble;
    if (!std::isfinite(number)) {
      error = (path.empty() ? std::string("the document") : "'" + path + "'") +
              " holds a number that is out of range";
      return false;
    }
    /* Every integer below 2^53 is a double that is exactly the number written,
       and at 2^53 the doubles are already farther apart than one: 2^53 and
       9007199254740993 are the same double. The boundary is therefore below
       2^53, which keeps the claim exact for every integer the model reports,
       instead of reporting 9007199254740992 for a text that said ...993. */
    if (index.kind(path) == JsonTokenKind::integer && std::fabs(number) < 9007199254740992.0) {
      value = JsonValue::make_integer(static_cast<long long>(number));
    } else {
      value = JsonValue::make_number(number);
    }
    return true;
  }
  if (cJSON_IsArray(item)) {
    JsonValue array = JsonValue::make_array();
    std::size_t position = 0;
    for (const cJSON *child = item->child; child != 0; child = child->next) {
      JsonValue element;
      if (!to_json_value(index, append_json_path_index(path, position), child, element, error)) {
        return false;
      }
      array.add(element);
      ++position;
    }
    value = array;
    return true;
  }
  if (cJSON_IsObject(item)) {
    JsonValue object = JsonValue::make_object();
    for (const cJSON *child = item->child; child != 0; child = child->next) {
      const std::string key = child->string != 0 ? child->string : std::string();
      JsonValue member;
      if (!to_json_value(index, append_json_path_key(path, key), child, member, error)) {
        return false;
      }
      object.add(key, member);
    }
    value = object;
    return true;
  }
  value = JsonValue::make_null();
  return true;
}

} /* namespace detail */

/* What a read does with an object key that appears more than once. */
enum class JsonDuplicateKeys {
  /* A repeated key is an error, which is the default: a parser that silently
     keeps one of the two turns a copy-paste mistake into a value nobody can find
     later, and no rule about which one wins makes that better. */
  reject,
  /* The last value is read and the key keeps the position of the first. A caller
     that reports the duplicates itself, and still has to describe the rest of the
     document, needs this; it is not a lenient mode, because the text has already
     passed the scanner, which reports every repeated name. */
  allow
};

/* Read one JSON document. Returns false and fills error when the text is not
   JSON, when a key repeats and duplicates is JsonDuplicateKeys::reject, when the
   backend refuses it or when a number is outside the range the backend can hold.
   The model is left untouched on failure, and any value is accepted as a
   document, including a number or a string at the top level. */
inline bool parse_json_text(const std::string &text, JsonValue &value, std::string &error,
                            JsonDuplicateKeys duplicates = JsonDuplicateKeys::reject) {
  /* The backend reads a C string, so a NUL byte would end its view of the text
     while the scanner still saw what came after it. The two would disagree about
     where the document ends, so the text is refused before either of them runs. */
  const std::size_t nul = text.find('\0');
  if (nul != std::string::npos) {
    error = "the text holds a NUL byte at offset " + decimal_text(nul) + ", which JSON text cannot";
    return false;
  }
  JsonIndex index;
  std::string scan_error;
  if (!JsonIndex::build(text, index, scan_error)) {
    error = "not valid JSON: " + scan_error;
    return false;
  }
  const std::string duplicated = index.duplicated_key_list();
  if (duplicates == JsonDuplicateKeys::reject && !duplicated.empty()) {
    error = "duplicated object key(s): " + duplicated;
    return false;
  }
  const detail::CJsonTree tree(cJSON_Parse(text.c_str()));
  if (tree.empty()) {
    error = "not valid JSON: the JSON backend rejected the document";
    return false;
  }
  JsonValue parsed;
  if (!detail::to_json_value(index, std::string(), tree.get(), parsed, error)) {
    return false;
  }
  value = parsed;
  return true;
}

} /* namespace jsonlens */

#endif /* JSONLENS_READ_HPP */
