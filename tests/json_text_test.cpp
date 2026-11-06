/*
 * json_text_test.cpp - Reading and writing JSON text with the C++ API.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * parse_json_text() and to_json_text() are the two halves of the gap that
 * config/value.hpp leaves open: the model can hold a document, but only the
 * backend can read one from text or write one to text.
 *
 * These tests pin both halves, and they pin the claim that makes the write worth
 * having: the text is the text of the backend, byte for byte, for both layouts.
 * A second printer in this library would be a second dialect, and these tests
 * would not notice; comparing against the backend is what notices.
 */
#include <jsonlens/detail/decimal.hpp>
#include <jsonlens/jsonlens.hpp>

#include <cstddef>
#include <limits>
#include <string>

#include "framework/selfcheck.hpp"

namespace {

/* A document that holds every kind the model has. */
jsonlens::JsonValue sample() {
  jsonlens::JsonValue object = jsonlens::JsonValue::make_object();
  object.add("none", jsonlens::JsonValue::make_null());
  object.add("flag", jsonlens::JsonValue::make_bool(true));
  object.add("count", jsonlens::JsonValue::make_integer(-42));
  object.add("scale", jsonlens::JsonValue::make_number(0.5));
  object.add("text", jsonlens::JsonValue::make_string("a\tb\"c"));
  jsonlens::JsonValue pads = jsonlens::JsonValue::make_array();
  pads.add(jsonlens::JsonValue::make_integer(1));
  pads.add(jsonlens::JsonValue::make_string("two"));
  pads.add(jsonlens::JsonValue::make_bool(false));
  object.add("pads", pads);
  object.add("empty_array", jsonlens::JsonValue::make_array());
  object.add("empty_object", jsonlens::JsonValue::make_object());
  return object;
}

/* The same document, built with the backend, to compare the two writers. */
cJSON *sample_tree() {
  cJSON *const object = cJSON_CreateObject();
  cJSON_AddNullToObject(object, "none");
  cJSON_AddTrueToObject(object, "flag");
  cJSON_AddNumberToObject(object, "count", -42);
  cJSON_AddNumberToObject(object, "scale", 0.5);
  cJSON_AddStringToObject(object, "text", "a\tb\"c");
  cJSON *const pads = cJSON_AddArrayToObject(object, "pads");
  cJSON_AddItemToArray(pads, cJSON_CreateNumber(1));
  cJSON_AddItemToArray(pads, cJSON_CreateString("two"));
  cJSON_AddItemToArray(pads, cJSON_CreateFalse());
  cJSON_AddArrayToObject(object, "empty_array");
  cJSON_AddObjectToObject(object, "empty_object");
  return object;
}

/* A document nested to depth levels of arrays. */
std::string nested(std::size_t depth) {
  std::string text;
  for (std::size_t index = 0; index < depth; ++index) {
    text += "[";
  }
  for (std::size_t index = 0; index < depth; ++index) {
    text += "]";
  }
  return text;
}

/* The same, with a value at the innermost level, which is one more level of
   nesting than an empty innermost container would be. */
std::string nested_with_value(std::size_t depth) {
  std::string text;
  for (std::size_t index = 0; index < depth; ++index) {
    text += "[";
  }
  text += "0";
  for (std::size_t index = 0; index < depth; ++index) {
    text += "]";
  }
  return text;
}

} /* namespace */

SELFCHECK_TEST(reading_keeps_every_kind_of_a_document) {
  const std::string text = "{\"none\":null,\"flag\":true,\"count\":-42,\"scale\":0.5,"
                           "\"text\":\"hi\",\"pads\":[1,2],\"empty\":{}}";
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text(text, value, error), error);
  SELFCHECK_CHECK(value.is_object());
  SELFCHECK_CHECK(value.member("none") != 0 && value.member("none")->is_null());
  SELFCHECK_CHECK(value.member("flag") != 0 && value.member("flag")->is_bool());
  SELFCHECK_CHECK(value.member("count") != 0 && value.member("count")->is_integer());
  SELFCHECK_CHECK(value.member("scale") != 0 && value.member("scale")->is_number());
  SELFCHECK_CHECK(value.member("text") != 0 && value.member("text")->is_string());
  SELFCHECK_CHECK(value.member("pads") != 0 && value.member("pads")->is_array());
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), value.member("pads")->size());
  SELFCHECK_CHECK(value.member("empty") != 0 && value.member("empty")->is_object());

  bool flag = false;
  SELFCHECK_CHECK(value.member("flag")->as_bool(flag));
  SELFCHECK_CHECK_EQ(true, flag);
  long long count = 0;
  SELFCHECK_CHECK(value.member("count")->as_integer(count));
  SELFCHECK_CHECK_EQ(static_cast<long long>(-42), count);
  double scale = 0.0;
  SELFCHECK_CHECK(value.member("scale")->as_number(scale));
  SELFCHECK_CHECK_NEAR(0.5, scale, 1e-12);
  std::string token;
  SELFCHECK_CHECK(value.member("text")->as_string(token));
  SELFCHECK_CHECK_STREQ(std::string("hi"), token);
}

/* The scanner decides this, not the value: the backend holds both as a double,
   so the text is the only place the difference between 12 and 12.0 survives. */
SELFCHECK_TEST(reading_keeps_an_integer_apart_from_a_double) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK_MSG(
      jsonlens::parse_json_text("{\"i\":12,\"d\":12.0,\"e\":1e2,\"n\":-3}", value, error), error);
  SELFCHECK_CHECK(value.member("i")->is_integer());
  SELFCHECK_CHECK(value.member("d")->is_number());
  SELFCHECK_CHECK(value.member("e")->is_number());
  SELFCHECK_CHECK(value.member("n")->is_integer());
  double number = 0.0;
  SELFCHECK_CHECK(value.member("e")->as_number(number));
  SELFCHECK_CHECK_NEAR(100.0, number, 1e-12);
  /* A number reads as a number whatever way it was written, which is the rule
     the rest of the model follows. */
  SELFCHECK_CHECK(value.member("i")->is_number());
}

SELFCHECK_TEST(reading_refuses_a_text_that_is_not_json) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text("", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "not valid JSON");
  SELFCHECK_CHECK(!jsonlens::parse_json_text("{", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "not valid JSON");
  /* The position is part of the message, because "not valid" alone is not a
     report a caller can act on. */
  SELFCHECK_CHECK_CONTAINS(error, "line 1 column 2");
  SELFCHECK_CHECK(!jsonlens::parse_json_text("nope", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "not valid JSON");
  /* JSON has no comments and no trailing commas, and the scanner is the reason
     a comment cannot slip through as if it were data. */
  SELFCHECK_CHECK(!jsonlens::parse_json_text("{\"a\":1 // one\n}", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "not valid JSON");
  SELFCHECK_CHECK(!jsonlens::parse_json_text("[1,2,]", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "not valid JSON");
}

SELFCHECK_TEST(reading_refuses_trailing_text) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text("{\"a\":1} {\"b\":2}", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "unexpected trailing data");
}

SELFCHECK_TEST(reading_refuses_a_key_that_repeats) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text("{\"a\":1,\"a\":2}", value, error));
  SELFCHECK_CHECK_STREQ(std::string("duplicated object key(s): a"), error);
  /* Every name is reported, once, in one list. */
  SELFCHECK_CHECK(!jsonlens::parse_json_text("{\"b\":1,\"a\":1,\"b\":2,\"a\":3}", value, error));
  SELFCHECK_CHECK_STREQ(std::string("duplicated object key(s): a, b"), error);
  /* A repeat inside an array of objects is still a repeat. */
  SELFCHECK_CHECK(!jsonlens::parse_json_text("[{\"a\":1,\"a\":2}]", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "duplicated object key(s): a");
}

SELFCHECK_TEST(reading_refuses_a_document_that_is_nested_too_deep) {
  jsonlens::JsonValue value;
  std::string error;
  /* A document at the limit reads, whether the innermost level holds a container
     or a value inside it. */
  SELFCHECK_CHECK_MSG(
      jsonlens::parse_json_text(nested(jsonlens::json_nesting_limit()), value, error), error);
  SELFCHECK_CHECK_MSG(
      jsonlens::parse_json_text(nested_with_value(jsonlens::json_nesting_limit()), value, error),
      error);
  /* One container more is refused, and the message names the limit. */
  SELFCHECK_CHECK(
      !jsonlens::parse_json_text(nested(jsonlens::json_nesting_limit() + 1), value, error));
  SELFCHECK_CHECK_CONTAINS(error, jsonlens::json_nesting_error());
  /* The value that was there before the failure is still there. */
  SELFCHECK_CHECK(value.is_array());
}

SELFCHECK_TEST(reading_leaves_the_model_alone_when_it_fails) {
  jsonlens::JsonValue value = jsonlens::JsonValue::make_string("before");
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text("{\"a\":1,\"a\":2}", value, error));
  SELFCHECK_CHECK(value.is_string());
  std::string text;
  SELFCHECK_CHECK(value.as_string(text));
  SELFCHECK_CHECK_STREQ(std::string("before"), text);
}

SELFCHECK_TEST(writing_keeps_the_insertion_order_of_the_keys) {
  jsonlens::JsonValue object = jsonlens::JsonValue::make_object();
  object.add("zulu", jsonlens::JsonValue::make_integer(1));
  object.add("alpha", jsonlens::JsonValue::make_integer(2));
  object.add("mike", jsonlens::JsonValue::make_integer(3));
  /* A key that is written again keeps its place, so the order is the order of
     the first write and never the order of the last one. */
  object.add("zulu", jsonlens::JsonValue::make_integer(4));
  SELFCHECK_CHECK_STREQ(std::string("{\"zulu\":4,\"alpha\":2,\"mike\":3}"),
                        jsonlens::to_json_text(object, false));
}

SELFCHECK_TEST(writing_formats_the_way_the_backend_formats) {
  jsonlens::JsonValue object = jsonlens::JsonValue::make_object();
  object.add("a", jsonlens::JsonValue::make_integer(1));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":1}"), jsonlens::to_json_text(object, false));
  SELFCHECK_CHECK_STREQ(std::string("{\n\t\"a\":\t1\n}"), jsonlens::to_json_text(object, true));
  SELFCHECK_CHECK_STREQ(std::string("{}"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_object(), false));
  SELFCHECK_CHECK_STREQ(std::string("{\n}"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_object(), true));
  SELFCHECK_CHECK_STREQ(std::string("[]"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_array(), false));
  /* An array stays on one line, however long it is; only the formatted text puts
     a space after each comma. */
  jsonlens::JsonValue pads = jsonlens::JsonValue::make_array();
  pads.add(jsonlens::JsonValue::make_integer(1));
  pads.add(jsonlens::JsonValue::make_integer(2));
  SELFCHECK_CHECK_STREQ(std::string("[1,2]"), jsonlens::to_json_text(pads, false));
  SELFCHECK_CHECK_STREQ(std::string("[1, 2]"), jsonlens::to_json_text(pads, true));
}

/* The whole point of writing through the backend: the two texts are one text. */
SELFCHECK_TEST(writing_writes_what_the_backend_writes) {
  const jsonlens::JsonValue model = sample();
  cJSON *const tree = sample_tree();
  char *formatted = cJSON_Print(tree);
  char *compact = cJSON_PrintUnformatted(tree);
  SELFCHECK_CHECK(formatted != 0 && compact != 0);
  if (formatted != 0 && compact != 0) {
    SELFCHECK_CHECK_STREQ(std::string(formatted), jsonlens::to_json_text(model, true));
    SELFCHECK_CHECK_STREQ(std::string(compact), jsonlens::to_json_text(model, false));
    /* The default is the formatted text, which is the one a person reads. */
    SELFCHECK_CHECK_STREQ(std::string(formatted), jsonlens::to_json_text(model));
  }
  cJSON_free(formatted);
  cJSON_free(compact);
  cJSON_Delete(tree);
}

SELFCHECK_TEST(writing_writes_a_number_that_is_not_a_number_as_null) {
  const double infinity = std::numeric_limits<double>::infinity();
  const double quiet_nan = std::numeric_limits<double>::quiet_NaN();
  SELFCHECK_CHECK_STREQ(std::string("null"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_number(infinity)));
  SELFCHECK_CHECK_STREQ(std::string("null"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_number(quiet_nan)));
  /* A number that is a whole number loses the fraction it was written with, and
     keeps its value. */
  SELFCHECK_CHECK_STREQ(std::string("12"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_number(12.0)));
  SELFCHECK_CHECK_STREQ(std::string("-0"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_number(-0.0)));
}

SELFCHECK_TEST(writing_refuses_a_string_that_holds_a_NUL_byte) {
  const std::string with_nul("a\0b", 3);
  jsonlens::JsonValue value = jsonlens::JsonValue::make_object();
  value.add("text", jsonlens::JsonValue::make_string(with_nul));
  value.add("fine", jsonlens::JsonValue::make_integer(1));
  std::string text;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::to_json_text(value, text, error, false));
  SELFCHECK_CHECK_CONTAINS(error, "text");
  SELFCHECK_CHECK_CONTAINS(error, "NUL");
  SELFCHECK_CHECK(text.empty());
  /* The writer with no error channel says the same thing in the only way it
     can, and a document that was written is never empty. */
  SELFCHECK_CHECK(jsonlens::to_json_text(value, false).empty());
  SELFCHECK_CHECK(!jsonlens::to_json_text(jsonlens::JsonValue::make_null(), false).empty());
}

SELFCHECK_TEST(writing_an_integer_is_exact_up_to_two_to_the_fifty_three) {
  const long long exact = 9007199254740992LL;
  SELFCHECK_CHECK_STREQ(std::string("9007199254740992"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_integer(exact)));
  /* One past it is the same double, and the backend cannot tell them apart, so
     the text is the rounded value. This is the limit of the pinned backend and
     not of this writer, and it is written down in the release notes. */
  SELFCHECK_CHECK_STREQ(std::string("9007199254740992"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_integer(exact + 1)));
  SELFCHECK_CHECK_STREQ(std::string("-42"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_integer(-42)));
  SELFCHECK_CHECK_STREQ(std::string("0"),
                        jsonlens::to_json_text(jsonlens::JsonValue::make_integer(0)));
  /* A large integer that is a round number of the backend is written in the
     exponent form of the backend, which is the same value. */
  SELFCHECK_CHECK_STREQ(
      std::string("1e+15"),
      jsonlens::to_json_text(jsonlens::JsonValue::make_integer(1000000000000000LL)));
}

SELFCHECK_TEST(a_document_survives_a_round_trip_through_text) {
  const jsonlens::JsonValue model = sample();
  const std::string compact = jsonlens::to_json_text(model, false);
  jsonlens::JsonValue again;
  std::string error;
  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text(compact, again, error), error);
  SELFCHECK_CHECK_STREQ(compact, jsonlens::to_json_text(again, false));
  /* The formatted text describes the same document. */
  jsonlens::JsonValue formatted;
  SELFCHECK_CHECK_MSG(
      jsonlens::parse_json_text(jsonlens::to_json_text(model, true), formatted, error), error);
  SELFCHECK_CHECK_STREQ(compact, jsonlens::to_json_text(formatted, false));
}
