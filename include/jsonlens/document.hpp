/*
 * document.hpp - One JSON document, addressed by path, with the C++ API.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * A JsonDocument is a value, not a handle. It owns a JsonValue, it copies when
 * it is copied, and it is destroyed with the scope that holds it, so there is no
 * tree to free, no reference to keep alive and no borrowed pointer to misuse.
 *
 * It is the answer to the question "how does a program read a configuration or a
 * report without meeting the C API of the backend?":
 *
 *   JsonDocument document;
 *   std::string error;
 *   if (!JsonDocument::load_file("report.json", document, error)) { return false; }
 *   long long port = 0;
 *   if (document.get_integer("device.port", port, error)) { ... }
 *   document.set("device.name", JsonValue::make_string("uart0"), error);
 *   document.save_file("report.json", error);
 *
 * Everything is addressed with the same path syntax the rest of the library
 * uses: "device.port", "pads[2].name". A leading '/' is ignored and means
 * nothing more, which is what makes the familiar "/device/port" style a rewrite
 * and not a second syntax. An empty path is the document itself, and a key
 * inside a path may not be empty, so a path is always a walk that can be taken.
 *
 * Reading is strict by default: a path that is not there, or a value of the
 * wrong kind, is a failure with a message that names the path and both kinds,
 * because silence is how a typo turns into a wrong configuration that nobody
 * notices. A parameter that has a default reads with one anyway: the call takes
 * the fallback, returns it instead of failing, and keeps the reason in
 * last_error(), so the caller can report that it fell back.
 *
 * Writing creates what is missing and refuses the rest. A key that is not there
 * is added, a key that is there keeps its place and takes the new value, and a
 * missing step in the middle is created as an object or an array according to
 * the step that follows it. A null in the middle is a step that carries nothing,
 * so it is replaced by what the step needs. Anything else - a number where the
 * path goes down - is a failure: the step that could not be walked through names
 * itself and the kind it needed there, and nothing is overwritten.
 *
 * A write copies the containers it walks, so a deep path in a large document
 * costs a copy per level. The documents this library is used for are small, and
 * the alternative is a mutable view, which is a handle to keep alive and a source
 * of dangling references; that is exactly what this class does not want to be.
 */
#ifndef JSONLENS_DOCUMENT_HPP
#define JSONLENS_DOCUMENT_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "jsonlens/detail/decimal.hpp"
#include "jsonlens/detail/text_file.hpp"
#include "jsonlens/json_index.hpp"
#include "jsonlens/read.hpp"
#include "jsonlens/value.hpp"
#include "jsonlens/write.hpp"

namespace jsonlens {

/* One JSON document. */
class JsonDocument {
public:
  /* A document starts as an empty object, which is what a builder wants. */
  JsonDocument() : root_(JsonValue::make_object()) {}

  static JsonDocument object() { return JsonDocument(); }

  static JsonDocument array() {
    JsonDocument document;
    document.root_ = JsonValue::make_array();
    return document;
  }

  /* A document around a value that already exists. */
  static JsonDocument of(const JsonValue &value) {
    JsonDocument document;
    document.root_ = value;
    return document;
  }

  /* Read one document from a text. Returns false and fills error when the text
     is not JSON, and leaves document untouched. */
  static bool parse(const std::string &text, JsonDocument &document, std::string &error) {
    JsonValue value;
    if (!parse_json_text(text, value, error)) {
      return false;
    }
    document.root_ = value;
    return true;
  }

  /* Read one document from a file. */
  static bool load_file(const std::string &path, JsonDocument &document, std::string &error) {
    std::string text;
    std::string problem;
    if (!read_text_file(path, text, problem)) {
      error = problem;
      return false;
    }
    return parse(text, document, error);
  }

  /* True when a value is at the path. */
  bool has(const std::string &path) const {
    const JsonValue *value = 0;
    std::string error;
    return locate(path, value, error);
  }

  /* The value at the path, or the null pointer when there is none. The pointer
     belongs to this document and lives until the document is changed or
     destroyed. */
  const JsonValue *find(const std::string &path) const {
    const JsonValue *value = 0;
    std::string error;
    if (!locate(path, value, error)) {
      return 0;
    }
    return value;
  }

  /* One value at a path. Every one of these fails when there is no value at the
     path or when the value has another kind. A number reads as a number whether
     it was written as an integer or with a fraction. */
  bool get_value(const std::string &path, JsonValue &value, std::string &error) const {
    last_error_.clear();
    const JsonValue *found = 0;
    if (!locate(path, found, error)) {
      return read_failed(error);
    }
    value = *found;
    return true;
  }

  bool get_bool(const std::string &path, bool &value, std::string &error) const {
    last_error_.clear();
    const JsonValue *found = 0;
    if (!require(path, JsonKind::boolean, found, error)) {
      return read_failed(error);
    }
    found->as_bool(value);
    return true;
  }

  bool get_integer(const std::string &path, long long &value, std::string &error) const {
    last_error_.clear();
    const JsonValue *found = 0;
    if (!require(path, JsonKind::integer, found, error)) {
      return read_failed(error);
    }
    found->as_integer(value);
    return true;
  }

  bool get_number(const std::string &path, double &value, std::string &error) const {
    last_error_.clear();
    const JsonValue *found = 0;
    if (!require(path, JsonKind::number, found, error)) {
      return read_failed(error);
    }
    found->as_number(value);
    return true;
  }

  bool get_string(const std::string &path, std::string &value, std::string &error) const {
    last_error_.clear();
    const JsonValue *found = 0;
    if (!require(path, JsonKind::string, found, error)) {
      return read_failed(error);
    }
    found->as_string(value);
    return true;
  }

  /* The same reads for a parameter that has a default: a value that is missing
     or of another kind gives the fallback, and last_error() still names the
     reason. The separate name keeps a call that wants a value in a variable from
     quietly becoming a call that returns a default, which is what happens when
     both shapes share one name. */
  bool get_bool_or(const std::string &path, bool fallback) const {
    bool value = false;
    std::string error;
    return get_bool(path, value, error) ? value : fallback;
  }

  long long get_integer_or(const std::string &path, long long fallback) const {
    long long value = 0;
    std::string error;
    return get_integer(path, value, error) ? value : fallback;
  }

  double get_number_or(const std::string &path, double fallback) const {
    double value = 0.0;
    std::string error;
    return get_number(path, value, error) ? value : fallback;
  }

  std::string get_string_or(const std::string &path, const std::string &fallback) const {
    std::string value;
    std::string error;
    return get_string(path, value, error) ? value : fallback;
  }

  /* A whole array at a path, as its items rather than one scalar. An empty path
     or "/" is the document itself, so a document whose root is an array reads
     here. Each item is a JsonValue, which then reads a member or a scalar. */
  bool get_array(const std::string &path, std::vector<JsonValue> &items, std::string &error) const {
    last_error_.clear();
    const JsonValue *found = 0;
    if (!require(path, JsonKind::array, found, error)) {
      return read_failed(error);
    }
    std::vector<JsonValue> read;
    read.reserve(found->size());
    for (std::size_t index = 0; index < found->size(); ++index) {
      read.push_back(*found->at(index));
    }
    items.swap(read);
    return true;
  }

  /* The same array as documents, so each item reads its fields by path the way a
     document does. This is the shape of a table of records, where every item is
     an object with optional fields of its own. */
  bool get_documents(const std::string &path, std::vector<JsonDocument> &items,
                     std::string &error) const;

  /* An array of scalars. False when the path holds no array or one item of
     another kind, in which case the item is named in the error and values keeps
     what it held before. */
  bool get_int_array(const std::string &path, std::vector<long long> &values,
                     std::string &error) const;
  bool get_number_array(const std::string &path, std::vector<double> &values,
                        std::string &error) const;
  bool get_string_array(const std::string &path, std::vector<std::string> &values,
                        std::string &error) const;

  /* Reason of the last read that failed, empty after a read that succeeded. The
     reads are const, so this one string is shared by every read of the same
     document; a program that reads one document from two threads at once should
     not ask for it. */
  const std::string &last_error() const { return last_error_; }

  /* Put a value at a path, creating the steps that are missing. An empty path
     replaces the whole document. */
  bool set(const std::string &path, const JsonValue &value, std::string &error) {
    std::vector<JsonPathStep> steps;
    if (!split_path(path, steps, error)) {
      return false;
    }
    if (steps.empty()) {
      root_ = value;
      return true;
    }
    return assign(root_, steps, 0, std::string(), value, error);
  }

  /* The document as JSON. An empty text means the document could not be
     written, which write.hpp describes. */
  std::string to_text(bool formatted = true) const { return to_json_text(root_, formatted); }

  /* Replace the contents of a file with the document. */
  bool save_file(const std::string &path, std::string &error, bool formatted = true) const {
    std::string text;
    std::string problem;
    if (!to_json_text(root_, text, problem, formatted)) {
      error = problem;
      return false;
    }
    if (!write_text_file(path, text, problem)) {
      error = problem;
      return false;
    }
    return true;
  }

  const JsonValue &root() const { return root_; }
  JsonValue &root() { return root_; }

private:
  /* Keep the reason a read failed, and report false for the read. */
  bool read_failed(std::string &error) const {
    last_error_ = error;
    return false;
  }

  /* Split a path and refuse one that is deeper than a document this library can
     read: such a path could never name a value, and refusing it here also keeps
     the walk below from costing more stack than one read of a document. */
  bool split_path(const std::string &path, std::vector<JsonPathStep> &steps,
                  std::string &error) const {
    if (!parse_json_path(path, steps)) {
      error = path + ": the path is not valid";
      return false;
    }
    if (steps.size() > json_nesting_limit()) {
      error = path + ": the path has " + detail::decimal_text(steps.size()) +
              " step(s), more than the limit of " + detail::decimal_text(json_nesting_limit());
      return false;
    }
    return true;
  }

  /* The value a step creates when the path has to go through it, chosen by the
     step that comes next: a key needs an object and an index needs an array. */
  static JsonValue fresh_container(const std::vector<JsonPathStep> &steps, std::size_t index) {
    return steps[index].is_index ? JsonValue::make_array() : JsonValue::make_object();
  }

  /* Walk a path from the root. Returns false and fills error when the path is
     not valid or when there is no value at it. */
  bool locate(const std::string &path, const JsonValue *&value, std::string &error) const {
    std::vector<JsonPathStep> steps;
    if (!split_path(path, steps, error)) {
      return false;
    }
    const JsonValue *node = &root_;
    for (std::size_t index = 0; index < steps.size(); ++index) {
      node = steps[index].is_index ? node->at(steps[index].index) : node->member(steps[index].key);
      if (node == 0) {
        error = path + ": there is no value at this path";
        return false;
      }
    }
    value = node;
    return true;
  }

  /* Walk a path and check the kind of the value that is there. */
  bool require(const std::string &path, JsonKind wanted, const JsonValue *&value,
               std::string &error) const {
    if (!locate(path, value, error)) {
      return false;
    }
    if (wanted == JsonKind::number) {
      /* An integer is a number, and reading it as one cannot lose anything. */
      if (!value->is_number()) {
        error = path + ": expected " + json_kind_name(wanted) + ", found " +
                json_kind_name(value->kind());
        return false;
      }
      return true;
    }
    if (value->kind() != wanted) {
      error = path + ": expected " + json_kind_name(wanted) + ", found " +
              json_kind_name(value->kind());
      return false;
    }
    return true;
  }

  /* Put value at steps[index] and below, where prefix is the path that reached
     node and error is the message of the first step that could not be taken. */
  bool assign(JsonValue &node, const std::vector<JsonPathStep> &steps, std::size_t index,
              const std::string &prefix, const JsonValue &value, std::string &error) const {
    const JsonPathStep &step = steps[index];
    const bool last = (index + 1) == steps.size();
    const std::string where = prefix.empty() ? std::string("the document") : prefix;

    if (step.is_index) {
      if (!node.is_array()) {
        error = where + ": expected an array, found " + json_kind_name(node.kind());
        return false;
      }
      if (step.index > node.size()) {
        error = where + ": the array holds " + detail::decimal_text(node.size()) + " item(s), so " +
                append_json_path_index(prefix, step.index) + " is out of range";
        return false;
      }
      if (last) {
        node.set_at(step.index, value);
        return true;
      }
      /* A value that is already there is walked through, and one that is missing
         or null is replaced by the container the next step needs. A value of
         another kind is left to the next step, which is the level that knows
         whether it wanted an object or an array. */
      const JsonValue *const existing = node.at(step.index);
      const std::string step_path = append_json_path_index(prefix, step.index);
      JsonValue child =
          (existing == 0 || existing->is_null()) ? fresh_container(steps, index + 1) : *existing;
      if (!assign(child, steps, index + 1, step_path, value, error)) {
        return false;
      }
      node.set_at(step.index, child);
      return true;
    }

    if (!node.is_object()) {
      error = where + ": expected an object, found " + json_kind_name(node.kind());
      return false;
    }
    if (last) {
      node.try_add(step.key, value);
      return true;
    }
    const JsonValue *const existing = node.member(step.key);
    const std::string step_path = append_json_path_key(prefix, step.key);
    JsonValue child =
        (existing == 0 || existing->is_null()) ? fresh_container(steps, index + 1) : *existing;
    if (!assign(child, steps, index + 1, step_path, value, error)) {
      return false;
    }
    node.try_add(step.key, child);
    return true;
  }

  JsonValue root_;
  mutable std::string last_error_;
};

/* The three array reads share one walk: take the array, then one item at a
   time, and stop at the first item of another kind. */
inline bool JsonDocument::get_documents(const std::string &path, std::vector<JsonDocument> &items,
                                        std::string &error) const {
  std::vector<JsonValue> values;
  if (!get_array(path, values, error)) {
    return false;
  }
  std::vector<JsonDocument> read;
  read.reserve(values.size());
  for (std::size_t index = 0; index < values.size(); ++index) {
    read.push_back(JsonDocument::of(values[index]));
  }
  items.swap(read);
  return true;
}

inline bool JsonDocument::get_int_array(const std::string &path, std::vector<long long> &values,
                                        std::string &error) const {
  std::vector<JsonValue> items;
  if (!get_array(path, items, error)) {
    return false;
  }
  std::vector<long long> read;
  read.reserve(items.size());
  for (std::size_t index = 0; index < items.size(); ++index) {
    long long value = 0;
    if (!items[index].as_integer(value)) {
      error = append_json_path_index(path, index) + ": expected an integer, found " +
              json_kind_name(items[index].kind());
      return read_failed(error);
    }
    read.push_back(value);
  }
  values.swap(read);
  return true;
}

inline bool JsonDocument::get_number_array(const std::string &path, std::vector<double> &values,
                                           std::string &error) const {
  std::vector<JsonValue> items;
  if (!get_array(path, items, error)) {
    return false;
  }
  std::vector<double> read;
  read.reserve(items.size());
  for (std::size_t index = 0; index < items.size(); ++index) {
    double value = 0.0;
    if (!items[index].as_number(value)) {
      error = append_json_path_index(path, index) + ": expected a number, found " +
              json_kind_name(items[index].kind());
      return read_failed(error);
    }
    read.push_back(value);
  }
  values.swap(read);
  return true;
}

inline bool JsonDocument::get_string_array(const std::string &path,
                                           std::vector<std::string> &values,
                                           std::string &error) const {
  std::vector<JsonValue> items;
  if (!get_array(path, items, error)) {
    return false;
  }
  std::vector<std::string> read;
  read.reserve(items.size());
  for (std::size_t index = 0; index < items.size(); ++index) {
    std::string value;
    if (!items[index].as_string(value)) {
      error = append_json_path_index(path, index) + ": expected a string, found " +
              json_kind_name(items[index].kind());
      return read_failed(error);
    }
    read.push_back(value);
  }
  values.swap(read);
  return true;
}

} /* namespace jsonlens */

#endif /* JSONLENS_DOCUMENT_HPP */
