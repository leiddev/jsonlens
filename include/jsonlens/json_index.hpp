/*
 * json_index.hpp - Token positions and token kinds of a JSON text.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * The JSON backend answers "what does this key hold", but it keeps no source
 * positions, and a caller that reports a mistake needs a line and a column. This
 * scanner supplies exactly that missing part: it walks the text once, records
 * where every key and every value starts, and notices duplicated object keys, of
 * which a JSON parser silently drops all but the last.
 *
 * It is deliberately not a JSON parser. It never converts a number, never
 * builds a document and never hands out a value; the backend stays the only
 * authority on what a key holds. What this scanner does decide is whether the
 * text follows JSON syntax, and there it is as strict as the reference
 * implementation: it rejects trailing data, raw control characters inside a
 * string and numbers such as 01 or .5, all of which a lenient parser accepts. It
 * also rejects what is not JSON text at all: an unexpected byte order mark, a
 * byte sequence that is not valid UTF-8, a \u escape that is half of a surrogate
 * pair and a \u0000 escape, which no C string can carry. A document therefore
 * has to satisfy both this scanner and the backend.
 *
 * Paths use the syntax the rest of the library uses: dot separated keys with
 * array subscripts in brackets, for example "device.pads[0].name". A key that
 * contains a dot, a bracket or a backslash is escaped with a backslash, see
 * escape_json_path_key().
 *
 * This header needs no JSON library, so a tool that only has to point at a
 * mistake in a file can use it on its own.
 */
#ifndef JSONLENS_JSON_INDEX_HPP
#define JSONLENS_JSON_INDEX_HPP

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "jsonlens/decimal.hpp"
#include "jsonlens/detail/byte_less.hpp"

namespace jsonlens {

/* How many containers of a document may be open at the same time. Both the
   scanner and the reader that turns the text into a JsonValue walk one stack
   frame per open container, about a kilobyte per frame in a debug build, so the
   limit is what one read of a pathological document may cost on the stack. The
   backend would accept CJSON_NESTING_LIMIT, which is 1000, so this is the
   stricter of the two and a document this scanner accepted is never refused by
   the backend for being too deep. Real documents nest a handful of levels. */
inline std::size_t json_nesting_limit() {
  return 64;
}

/* Message of the one failure that reports the limit. */
inline std::string json_nesting_error() {
  return "nesting is deeper than the limit of " + decimal_text(json_nesting_limit());
}

/* What a path points at. */
enum class JsonTokenKind {
  missing = 0, /* nothing was recorded for this path */
  object,
  array,
  string,
  integer, /* a number written without a fraction and without an exponent */
  number,  /* any other number */
  boolean,
  null_value
};

/* Lower case name of a token kind, for messages and for tests. */
inline const char *json_token_kind_name(JsonTokenKind kind) {
  switch (kind) {
  case JsonTokenKind::object:
    return "object";
  case JsonTokenKind::array:
    return "array";
  case JsonTokenKind::string:
    return "string";
  case JsonTokenKind::integer:
    return "integer";
  case JsonTokenKind::number:
    return "number";
  case JsonTokenKind::boolean:
    return "boolean";
  case JsonTokenKind::null_value:
    return "null";
  case JsonTokenKind::missing:
    break;
  }
  return "missing";
}

/* A one based line and column inside the scanned text. */
struct JsonPosition {
  std::size_t line;
  std::size_t column;
};

/* A position that was never determined. */
inline JsonPosition unknown_json_position() {
  JsonPosition position;
  position.line = 0;
  position.column = 0;
  return position;
}

/* True when a position was actually determined. */
inline bool json_position_is_known(const JsonPosition &position) {
  return position.line != 0;
}

/* Where one path lives and what it holds. */
struct JsonEntry {
  JsonPosition key;   /* Start of the key token, unknown for array elements. */
  JsonPosition value; /* Start of the value token. */
  JsonTokenKind kind;

  JsonEntry() : kind(JsonTokenKind::missing) {
    key = unknown_json_position();
    value = unknown_json_position();
  }
};

/* Escape one key so that it can become a segment of a path. */
inline std::string escape_json_path_key(const std::string &key) {
  std::string escaped;
  for (std::size_t index = 0; index < key.size(); ++index) {
    const char character = key[index];
    if (character == '.' || character == '[' || character == ']' || character == '\\') {
      escaped.push_back('\\');
    }
    escaped.push_back(character);
  }
  return escaped;
}

/* Append one key to a path. */
inline std::string append_json_path_key(const std::string &path, const std::string &key) {
  if (path.empty()) {
    return escape_json_path_key(key);
  }
  return path + "." + escape_json_path_key(key);
}

/* Append one array subscript to a path. */
inline std::string append_json_path_index(const std::string &path, std::size_t index) {
  return path + "[" + decimal_text(index) + "]";
}

/* One step of a relative path: an object key or an array subscript. */
struct JsonPathStep {
  bool is_index;
  std::string key;
  std::size_t index;

  JsonPathStep() : is_index(false), index(0) {}
};

/* Split "net.port" or "pads[2].name" into steps. A leading '/' is ignored, so
   the familiar absolute style "/log_enable" also works. An empty path yields
   no step at all and means "the value itself". A backslash escapes the next
   character, which allows a key that contains a dot.
 *
 * A key may not be empty and a key may not follow a subscript without a '.' in
 * front of it, so "a.", "a..b" and "a[0]b" are refused instead of standing for
 * something surprising. The empty key is out of reach on purpose, because the
 * empty path already means the whole document. */
inline bool parse_json_path(const std::string &path, std::vector<JsonPathStep> &steps) {
  steps.clear();
  std::size_t offset = 0;
  if (offset < path.size() && path[offset] == '/') {
    ++offset;
  }
  std::string key;
  bool after_index = false;
  while (offset < path.size()) {
    const char character = path[offset];
    if (after_index && character != '.' && character != '[') {
      return false;
    }
    if (character == '\\') {
      if (offset + 1 >= path.size()) {
        return false;
      }
      key.push_back(path[offset + 1]);
      offset += 2;
      continue;
    }
    if (character == '.') {
      if (!after_index) {
        if (key.empty()) {
          return false;
        }
        JsonPathStep step;
        step.key = key;
        steps.push_back(step);
      }
      key.clear();
      after_index = false;
      ++offset;
      continue;
    }
    if (character == '[') {
      if (!key.empty()) {
        JsonPathStep step;
        step.key = key;
        steps.push_back(step);
        key.clear();
      }
      const std::size_t end = path.find(']', offset + 1);
      if (end == std::string::npos || end == offset + 1) {
        return false;
      }
      std::size_t value = 0;
      for (std::size_t cursor = offset + 1; cursor < end; ++cursor) {
        if (path[cursor] < '0' || path[cursor] > '9') {
          return false;
        }
        value = value * 10 + static_cast<std::size_t>(path[cursor] - '0');
      }
      JsonPathStep step;
      step.is_index = true;
      step.index = value;
      steps.push_back(step);
      offset = end + 1;
      after_index = true;
      continue;
    }
    key.push_back(character);
    ++offset;
  }
  if (!key.empty()) {
    JsonPathStep step;
    step.key = key;
    steps.push_back(step);
  } else if (!after_index && !steps.empty()) {
    /* The path ends where a key was expected. */
    return false;
  }
  return true;
}

namespace detail {
class JsonScanner;
}

/* The positions, kinds and duplicate keys of one text. */
class JsonIndex {
public:
  /* Scan a text. Returns false and fills error with "message at line L column
     C" when the text is not one JSON value as far as this scanner is
     concerned. */
  static bool build(const std::string &text, JsonIndex &index, std::string &error);

  /* Everything known about a path. kind is missing when nothing was recorded. */
  JsonEntry entry(const std::string &path) const {
    const std::map<std::string, JsonEntry, ByteLess>::const_iterator found = entries_.find(path);
    if (found == entries_.end()) {
      return JsonEntry();
    }
    return found->second;
  }

  JsonTokenKind kind(const std::string &path) const { return entry(path).kind; }

  /* Start of the value at a path, unknown when the path is not recorded. */
  JsonPosition value_position(const std::string &path) const { return entry(path).value; }

  /* Start of the key at a path, unknown for array elements and the root. */
  JsonPosition key_position(const std::string &path) const { return entry(path).key; }

  bool has(const std::string &path) const { return entries_.find(path) != entries_.end(); }

  /* Names of the object keys that were written more than once, in the order in
     which the repetition was found, without repetitions themselves. */
  const std::vector<std::string> &duplicate_keys() const { return duplicates_; }

  /* The same names as one sorted list, separated by ", ", or an empty string
     when no key repeats. This is the text an error message carries, so it is
     built in one place: the document reports it and the reader refuses the text
     with it. */
  std::string duplicated_key_list() const {
    std::vector<std::string> names(duplicates_);
    std::sort(names.begin(), names.end(), ByteLess());
    std::string list;
    for (std::size_t index = 0; index < names.size(); ++index) {
      if (index != 0) {
        list += ", ";
      }
      list += names[index];
    }
    return list;
  }

  std::size_t size() const { return entries_.size(); }

  /* Called by the scanner while it walks the text. */
  void record(const std::string &path, const JsonPosition &key_position,
              const JsonPosition &value_position, JsonTokenKind kind) {
    JsonEntry entry;
    entry.key = key_position;
    entry.value = value_position;
    entry.kind = kind;
    entries_[path] = entry;
  }

  /* Called by the scanner for every key it has already seen in one object. */
  void record_duplicate_key(const std::string &key) {
    for (std::size_t index = 0; index < duplicates_.size(); ++index) {
      if (duplicates_[index] == key) {
        return;
      }
    }
    duplicates_.push_back(key);
  }

private:
  std::map<std::string, JsonEntry, ByteLess> entries_;
  std::vector<std::string> duplicates_;
};

namespace detail {

/* One pass over the text, collecting positions, kinds and duplicate keys. */
class JsonScanner {
public:
  JsonScanner(const std::string &text, JsonIndex &sink)
      : text_(text), sink_(sink), offset_(0), line_(1), column_(1) {}

  /* True when the whole text is one JSON value. */
  bool run() {
    /* A byte order mark is not part of the text: it is what an editor put in
       front of it. RFC 8259 forbids adding one, and accepting it here would mean
       a document that reads back differently from the one that was written. */
    if (text_.size() >= 3 && static_cast<unsigned char>(text_[0]) == 0xefU &&
        static_cast<unsigned char>(text_[1]) == 0xbbU &&
        static_cast<unsigned char>(text_[2]) == 0xbfU) {
      return fail("an unexpected UTF-8 byte order mark");
    }
    if (!parse_value(std::string(), 0, unknown_json_position())) {
      return false;
    }
    skip_whitespace();
    if (offset_ != text_.size()) {
      return fail("unexpected trailing data");
    }
    return true;
  }

  /* "message at line L column C", set when run() returned false. */
  const std::string &error() const { return error_; }

private:
  JsonPosition here() const {
    JsonPosition position;
    position.line = line_;
    position.column = column_;
    return position;
  }

  char peek() const { return offset_ < text_.size() ? text_[offset_] : '\0'; }

  void advance() {
    const char character = text_[offset_];
    ++offset_;
    if (character == '\n') {
      ++line_;
      column_ = 1;
    } else {
      ++column_;
    }
  }

  void skip_whitespace() {
    while (offset_ < text_.size()) {
      const char character = text_[offset_];
      if (character != ' ' && character != '\t' && character != '\n' && character != '\r') {
        return;
      }
      advance();
    }
  }

  bool fail(const std::string &message) {
    error_ = message + " at line " + decimal_text(line_) + " column " + decimal_text(column_);
    return false;
  }

  static bool is_digit(char character) { return character >= '0' && character <= '9'; }

  static bool is_hex(char character) {
    return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F');
  }

  static unsigned long hex_value(char character) {
    if (character >= '0' && character <= '9') {
      return static_cast<unsigned long>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
      return static_cast<unsigned long>(character - 'a' + 10);
    }
    return static_cast<unsigned long>(character - 'A' + 10);
  }

  /* Append one code point as UTF-8. The caller has already rejected a lone
     surrogate, so every code point here is a character. */
  static void append_utf8(std::string &out, unsigned long code) {
    if (code < 0x80UL) {
      out.push_back(static_cast<char>(code));
    } else if (code < 0x800UL) {
      out.push_back(static_cast<char>(0xc0UL | (code >> 6)));
      out.push_back(static_cast<char>(0x80UL | (code & 0x3fUL)));
    } else if (code < 0x10000UL) {
      out.push_back(static_cast<char>(0xe0UL | (code >> 12)));
      out.push_back(static_cast<char>(0x80UL | ((code >> 6) & 0x3fUL)));
      out.push_back(static_cast<char>(0x80UL | (code & 0x3fUL)));
    } else {
      out.push_back(static_cast<char>(0xf0UL | (code >> 18)));
      out.push_back(static_cast<char>(0x80UL | ((code >> 12) & 0x3fUL)));
      out.push_back(static_cast<char>(0x80UL | ((code >> 6) & 0x3fUL)));
      out.push_back(static_cast<char>(0x80UL | (code & 0x3fUL)));
    }
  }

  /* Four hexadecimal digits, the cursor is on the first of them. */
  bool read_hex4(unsigned long &value) {
    value = 0;
    for (int count = 0; count < 4; ++count) {
      if (offset_ >= text_.size() || !is_hex(text_[offset_])) {
        return fail("a \\u escape needs four hexadecimal digits");
      }
      value = (value << 4) | hex_value(text_[offset_]);
      advance();
    }
    return true;
  }

  /* One code point from a \u escape, which may be a surrogate pair. A lone
     surrogate is half a character and the model has nowhere to put it, so it is
     an error here rather than three bytes that are not UTF-8. */
  bool read_code_point(unsigned long &code) {
    if (code == 0UL) {
      return fail("a \\u0000 escape, which is a NUL byte this model cannot carry");
    }
    if (code >= 0xdc00UL && code <= 0xdfffUL) {
      return fail("a trailing surrogate escape without a leading one");
    }
    if (code < 0xd800UL || code > 0xdbffUL) {
      return true;
    }
    if (peek() != '\\') {
      return fail("a leading surrogate escape without a trailing one");
    }
    advance();
    if (peek() != 'u') {
      return fail("a leading surrogate escape without a trailing one");
    }
    advance();
    unsigned long trailing = 0;
    if (!read_hex4(trailing)) {
      return false;
    }
    if (trailing < 0xdc00UL || trailing > 0xdfffUL) {
      return fail("a leading surrogate escape without a trailing one");
    }
    code = 0x10000UL + ((code - 0xd800UL) << 10) + (trailing - 0xdc00UL);
    return true;
  }

  /* A byte above 0x7f starts a sequence in the text itself. It is checked here
     instead of trusted, because the model promises text that is valid UTF-8 from
     the file to the writer: an overlong form, a surrogate or a code point above
     U+10FFFF is what makes two different byte sequences name one character. */
  bool append_raw_utf8(std::string &out) {
    const std::size_t start = offset_;
    const unsigned char lead = static_cast<unsigned char>(text_[offset_]);
    std::size_t length = 0;
    unsigned char first_min = 0x80U;
    unsigned char first_max = 0xbfU;
    if (lead >= 0xc2U && lead <= 0xdfU) {
      length = 2;
    } else if (lead == 0xe0U) {
      length = 3;
      first_min = 0xa0U;
    } else if ((lead >= 0xe1U && lead <= 0xecU) || (lead >= 0xeeU && lead <= 0xefU)) {
      length = 3;
    } else if (lead == 0xedU) {
      length = 3;
      first_max = 0x9fU;
    } else if (lead == 0xf0U) {
      length = 4;
      first_min = 0x90U;
    } else if (lead >= 0xf1U && lead <= 0xf3U) {
      length = 4;
    } else if (lead == 0xf4U) {
      length = 4;
      first_max = 0x8fU;
    } else {
      return fail("a byte that starts no UTF-8 sequence in a string");
    }
    if (text_.size() - offset_ < length) {
      return fail("an incomplete UTF-8 sequence in a string");
    }
    const unsigned char first = static_cast<unsigned char>(text_[offset_ + 1]);
    if (first < first_min || first > first_max) {
      return fail("an invalid UTF-8 sequence in a string");
    }
    for (std::size_t index = 2; index < length; ++index) {
      const unsigned char other = static_cast<unsigned char>(text_[offset_ + index]);
      if (other < 0x80U || other > 0xbfU) {
        return fail("an invalid UTF-8 sequence in a string");
      }
    }
    for (std::size_t index = 0; index < length; ++index) {
      out.push_back(text_[start + index]);
      advance();
    }
    return true;
  }

  bool parse_string(std::string &out) {
    if (peek() != '"') {
      return fail("expected a string");
    }
    advance();
    out.clear();
    while (offset_ < text_.size()) {
      const char character = text_[offset_];
      if (character == '"') {
        advance();
        return true;
      }
      if (static_cast<unsigned char>(character) < 0x20U) {
        return fail("a raw control character in a string");
      }
      if (static_cast<unsigned char>(character) >= 0x80U) {
        if (!append_raw_utf8(out)) {
          return false;
        }
        continue;
      }
      if (character != '\\') {
        out.push_back(character);
        advance();
        continue;
      }
      advance();
      if (offset_ >= text_.size()) {
        return fail("an escape sequence at the end of the text");
      }
      const char escape = text_[offset_];
      if (escape == 'u') {
        advance();
        unsigned long code = 0;
        if (!read_hex4(code)) {
          return false;
        }
        if (!read_code_point(code)) {
          return false;
        }
        append_utf8(out, code);
        continue;
      }
      switch (escape) {
      case '"':
        out.push_back('"');
        break;
      case '\\':
        out.push_back('\\');
        break;
      case '/':
        out.push_back('/');
        break;
      case 'b':
        out.push_back('\b');
        break;
      case 'f':
        out.push_back('\f');
        break;
      case 'n':
        out.push_back('\n');
        break;
      case 'r':
        out.push_back('\r');
        break;
      case 't':
        out.push_back('\t');
        break;
      default:
        return fail("an unknown escape sequence in a string");
      }
      advance();
    }
    return fail("an unterminated string");
  }

  bool parse_number(JsonTokenKind &kind) {
    bool integer = true;
    if (peek() == '-') {
      advance();
    }
    if (peek() == '0') {
      advance();
    } else if (is_digit(peek())) {
      while (is_digit(peek())) {
        advance();
      }
    } else {
      return fail("a number needs a digit");
    }
    if (peek() == '.') {
      integer = false;
      advance();
      if (!is_digit(peek())) {
        return fail("a fraction needs a digit");
      }
      while (is_digit(peek())) {
        advance();
      }
    }
    if (peek() == 'e' || peek() == 'E') {
      integer = false;
      advance();
      if (peek() == '+' || peek() == '-') {
        advance();
      }
      if (!is_digit(peek())) {
        return fail("an exponent needs a digit");
      }
      while (is_digit(peek())) {
        advance();
      }
    }
    kind = integer ? JsonTokenKind::integer : JsonTokenKind::number;
    return true;
  }

  bool parse_literal(const char *text) {
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
      if (peek() != *cursor) {
        return fail("an unknown token");
      }
      advance();
    }
    return true;
  }

  bool parse_object(const std::string &path, std::size_t depth, const JsonPosition &key_position,
                    const JsonPosition &value_position) {
    advance(); /* '{' */
    std::map<std::string, bool, ByteLess> seen;
    skip_whitespace();
    if (peek() == '}') {
      advance();
    } else {
      for (;;) {
        skip_whitespace();
        if (peek() != '"') {
          return fail("expected a key string");
        }
        const JsonPosition member_key = here();
        std::string key;
        if (!parse_string(key)) {
          return false;
        }
        if (seen.find(key) != seen.end()) {
          sink_.record_duplicate_key(key);
        } else {
          seen[key] = true;
        }
        const std::string member_path = append_json_path_key(path, key);
        skip_whitespace();
        if (peek() != ':') {
          return fail("expected ':' after a key");
        }
        advance();
        if (!parse_value(member_path, depth + 1, member_key)) {
          return false;
        }
        skip_whitespace();
        const char character = peek();
        if (character == ',') {
          advance();
          continue;
        }
        if (character == '}') {
          advance();
          break;
        }
        return fail("expected ',' or '}'");
      }
    }
    sink_.record(path, key_position, value_position, JsonTokenKind::object);
    return true;
  }

  bool parse_array(const std::string &path, std::size_t depth, const JsonPosition &key_position,
                   const JsonPosition &value_position) {
    advance(); /* '[' */
    std::size_t index = 0;
    skip_whitespace();
    if (peek() == ']') {
      advance();
    } else {
      for (;;) {
        if (!parse_value(append_json_path_index(path, index), depth + 1, unknown_json_position())) {
          return false;
        }
        ++index;
        skip_whitespace();
        const char character = peek();
        if (character == ',') {
          advance();
          continue;
        }
        if (character == ']') {
          advance();
          break;
        }
        return fail("expected ',' or ']'");
      }
    }
    sink_.record(path, key_position, value_position, JsonTokenKind::array);
    return true;
  }

  bool parse_value(const std::string &path, std::size_t depth, const JsonPosition &key_position) {
    skip_whitespace();
    if (offset_ >= text_.size()) {
      return fail("unexpected end of the text");
    }
    const JsonPosition value_position = here();
    const char character = peek();
    if (character == '{') {
      if (depth >= json_nesting_limit()) {
        return fail(json_nesting_error());
      }
      return parse_object(path, depth, key_position, value_position);
    }
    if (character == '[') {
      if (depth >= json_nesting_limit()) {
        return fail(json_nesting_error());
      }
      return parse_array(path, depth, key_position, value_position);
    }
    if (character == '"') {
      std::string text;
      if (!parse_string(text)) {
        return false;
      }
      sink_.record(path, key_position, value_position, JsonTokenKind::string);
      return true;
    }
    if (character == 't' || character == 'f') {
      if (!parse_literal(character == 't' ? "true" : "false")) {
        return false;
      }
      sink_.record(path, key_position, value_position, JsonTokenKind::boolean);
      return true;
    }
    if (character == 'n') {
      if (!parse_literal("null")) {
        return false;
      }
      sink_.record(path, key_position, value_position, JsonTokenKind::null_value);
      return true;
    }
    if (character == '-' || is_digit(character)) {
      JsonTokenKind kind = JsonTokenKind::number;
      if (!parse_number(kind)) {
        return false;
      }
      sink_.record(path, key_position, value_position, kind);
      return true;
    }
    return fail("unexpected character");
  }

  const std::string &text_;
  JsonIndex &sink_;
  std::size_t offset_;
  std::size_t line_;
  std::size_t column_;
  std::string error_;
};

} /* namespace detail */

inline bool JsonIndex::build(const std::string &text, JsonIndex &index, std::string &error) {
  detail::JsonScanner scanner(text, index);
  if (!scanner.run()) {
    error = scanner.error();
    return false;
  }
  return true;
}

} /* namespace jsonlens */

#endif /* JSONLENS_JSON_INDEX_HPP */
