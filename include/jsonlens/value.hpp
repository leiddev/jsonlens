/*
 * value.hpp - One element specific parameter, as a value of a small tree.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * The schema cannot describe the parameters an element invents for itself, so
 * the configuration model keeps them as they were written. They are kept as a
 * tree of plain values instead of as a parsed document, which keeps the model
 * free of the JSON backend: a unit test builds parameters by hand, and
 * ElementConfig answers without a parser.
 *
 * A kind tells an integer from a number. Only the strict scanner of the loader
 * can: a backend reads both as one double, so 3 and 3.0 look alike afterwards.
 */
#ifndef JSONLENS_VALUE_HPP
#define JSONLENS_VALUE_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace jsonlens {

/* Kind of one value of a parameter. */
enum class JsonKind { null_value = 0, boolean, integer, number, string, array, object };

/* Name of a kind, as it reads inside a message. */
inline const char *json_kind_name(JsonKind kind) {
  switch (kind) {
  case JsonKind::null_value:
    return "null";
  case JsonKind::boolean:
    return "a boolean";
  case JsonKind::integer:
    return "an integer";
  case JsonKind::number:
    return "a number";
  case JsonKind::string:
    return "a string";
  case JsonKind::array:
    return "an array";
  case JsonKind::object:
    return "an object";
  }
  return "a value";
}

/* One parameter value. An array keeps its items and an object its members in
   document order, so a caller sees the configuration as it was written. */
class JsonValue {
public:
  /* A null value. */
  JsonValue()
      : kind_(JsonKind::null_value), boolean_(false), integer_(0), number_(0.0), text_(), items_(),
        members_() {}

  static JsonValue make_null() { return JsonValue(); }

  static JsonValue make_bool(bool value) {
    JsonValue item;
    item.kind_ = JsonKind::boolean;
    item.boolean_ = value;
    return item;
  }

  static JsonValue make_integer(long long value) {
    JsonValue item;
    item.kind_ = JsonKind::integer;
    item.integer_ = value;
    item.number_ = static_cast<double>(value);
    return item;
  }

  static JsonValue make_number(double value) {
    JsonValue item;
    item.kind_ = JsonKind::number;
    item.number_ = value;
    return item;
  }

  static JsonValue make_string(const std::string &value) {
    JsonValue item;
    item.kind_ = JsonKind::string;
    item.text_ = value;
    return item;
  }

  static JsonValue make_array() {
    JsonValue item;
    item.kind_ = JsonKind::array;
    return item;
  }

  static JsonValue make_object() {
    JsonValue item;
    item.kind_ = JsonKind::object;
    return item;
  }

  JsonKind kind() const { return kind_; }

  bool is_null() const { return kind_ == JsonKind::null_value; }
  bool is_bool() const { return kind_ == JsonKind::boolean; }
  bool is_integer() const { return kind_ == JsonKind::integer; }
  bool is_number() const { return kind_ == JsonKind::number || kind_ == JsonKind::integer; }
  bool is_string() const { return kind_ == JsonKind::string; }
  bool is_array() const { return kind_ == JsonKind::array; }
  bool is_object() const { return kind_ == JsonKind::object; }

  /* Scalars, false when the value has another kind. A number reads as a number
     only when it was written with a fraction or an exponent. */
  bool as_bool(bool &value) const;
  bool as_integer(long long &value) const;
  bool as_number(double &value) const;
  bool as_string(std::string &value) const;

  /* The same reads for a field that has a default, so a table with optional
     fields reads one item at a time without a guard per field. */
  bool get_bool_or(bool fallback) const;
  long long get_integer_or(long long fallback) const;
  double get_number_or(double fallback) const;
  std::string get_string_or(const std::string &fallback) const;

  /* Items of an array, members of an object, zero for every other kind. */
  std::size_t size() const;
  bool empty() const { return size() == 0; }

  /* Item of an array, the null pointer when the index is out of range. */
  const JsonValue *at(std::size_t index) const;

  /* Member of an object, the null pointer when the key is absent. */
  const JsonValue *member(const std::string &key) const;

  /* Keys of an object, in document order, empty for every other kind. */
  std::vector<std::string> keys() const;

  /* Appends an item to an array, adds a member to an object. Both ignore a
     value of another kind. A repeated key keeps its place and the last value
     wins, which is how the reference reader folds a duplicated key. */
  void add(const JsonValue &value);
  void add(const std::string &key, const JsonValue &value);

  /* The same two writes, but they report whether the value went in, because a
     caller that builds a document for something else to read wants to know when
     a value was addressed at a container that could not hold it. */
  bool try_add(const JsonValue &value);
  bool try_add(const std::string &key, const JsonValue &value);

  /* Replaces the item at a position of an array, keeping its place. A position
     equal to the size appends, which is how a builder grows an array one item at
     a time. Returns false when this value is not an array, or when the position
     is past the end. */
  bool set_at(std::size_t position, const JsonValue &value);

private:
  JsonKind kind_;
  bool boolean_;
  long long integer_;
  double number_;
  std::string text_;
  std::vector<JsonValue> items_;
  std::vector<std::pair<std::string, JsonValue>> members_;
};

inline bool JsonValue::as_bool(bool &value) const {
  if (kind_ != JsonKind::boolean) {
    return false;
  }
  value = boolean_;
  return true;
}

inline bool JsonValue::as_integer(long long &value) const {
  if (kind_ != JsonKind::integer) {
    return false;
  }
  value = integer_;
  return true;
}

inline bool JsonValue::as_number(double &value) const {
  if (kind_ != JsonKind::number && kind_ != JsonKind::integer) {
    return false;
  }
  value = number_;
  return true;
}

inline bool JsonValue::as_string(std::string &value) const {
  if (kind_ != JsonKind::string) {
    return false;
  }
  value = text_;
  return true;
}

inline bool JsonValue::get_bool_or(bool fallback) const {
  bool value = false;
  return as_bool(value) ? value : fallback;
}

inline long long JsonValue::get_integer_or(long long fallback) const {
  long long value = 0;
  return as_integer(value) ? value : fallback;
}

inline double JsonValue::get_number_or(double fallback) const {
  double value = 0.0;
  return as_number(value) ? value : fallback;
}

inline std::string JsonValue::get_string_or(const std::string &fallback) const {
  std::string value;
  return as_string(value) ? value : fallback;
}

inline std::size_t JsonValue::size() const {
  if (kind_ == JsonKind::array) {
    return items_.size();
  }
  if (kind_ == JsonKind::object) {
    return members_.size();
  }
  return 0;
}

inline const JsonValue *JsonValue::at(std::size_t index) const {
  if (kind_ != JsonKind::array || index >= items_.size()) {
    return 0;
  }
  return &items_[index];
}

inline const JsonValue *JsonValue::member(const std::string &key) const {
  if (kind_ != JsonKind::object) {
    return 0;
  }
  for (std::size_t index = 0; index < members_.size(); ++index) {
    if (members_[index].first == key) {
      return &members_[index].second;
    }
  }
  return 0;
}

inline std::vector<std::string> JsonValue::keys() const {
  std::vector<std::string> names;
  if (kind_ != JsonKind::object) {
    return names;
  }
  names.reserve(members_.size());
  for (std::size_t index = 0; index < members_.size(); ++index) {
    names.push_back(members_[index].first);
  }
  return names;
}

inline void JsonValue::add(const JsonValue &value) {
  if (kind_ == JsonKind::array) {
    items_.push_back(value);
  }
}

inline void JsonValue::add(const std::string &key, const JsonValue &value) {
  if (kind_ != JsonKind::object) {
    return;
  }
  for (std::size_t index = 0; index < members_.size(); ++index) {
    if (members_[index].first == key) {
      members_[index].second = value;
      return;
    }
  }
  members_.push_back(std::make_pair(key, value));
}

inline bool JsonValue::try_add(const JsonValue &value) {
  if (kind_ != JsonKind::array) {
    return false;
  }
  items_.push_back(value);
  return true;
}

inline bool JsonValue::try_add(const std::string &key, const JsonValue &value) {
  if (kind_ != JsonKind::object) {
    return false;
  }
  for (std::size_t index = 0; index < members_.size(); ++index) {
    if (members_[index].first == key) {
      members_[index].second = value;
      return true;
    }
  }
  members_.push_back(std::make_pair(key, value));
  return true;
}

inline bool JsonValue::set_at(std::size_t position, const JsonValue &value) {
  if (kind_ != JsonKind::array || position > items_.size()) {
    return false;
  }
  if (position == items_.size()) {
    items_.push_back(value);
    return true;
  }
  items_[position] = value;
  return true;
}

} /* namespace jsonlens */

#endif /* JSONLENS_VALUE_HPP */
