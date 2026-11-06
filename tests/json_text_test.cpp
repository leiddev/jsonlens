/*
 * json_text_test.cpp - Reading and writing JSON text with the C++ API.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * parse_json_text() and to_json_text() are the two halves of the gap that
 * value.hpp leaves open: the model can hold a document, but only the backend
 * can read one from text or write one to text.
 *
 * These tests pin both halves, and they pin the claim that makes the write worth
 * having: the text is the text of the backend, byte for byte, for both layouts.
 * A second printer in this library would be a second dialect, and these tests
 * would not notice; comparing against the backend is what notices.
 */
#include <jsonlens/decimal.hpp>
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

/* The backend reads a C string, so a NUL byte would end its view of the text
   while the scanner still saw what came after it. The two would then disagree
   about where the document ends, and the shorter reading would win silently. */
SELFCHECK_TEST(a_text_with_a_nul_byte_is_rejected) {
  std::string text("{\"a\":1}");
  text.push_back('\0');
  text += "{\"b\":2}";
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text(text, value, error));
  SELFCHECK_CHECK_CONTAINS(error, "NUL");
  SELFCHECK_CHECK_CONTAINS(error, "offset 7");
  SELFCHECK_CHECK(value.is_null());

  /* Inside a string is the same thing: the text, not the escape, is what holds
     the byte. */
  std::string inside("\"a");
  inside.push_back('\0');
  inside += "b\"";
  SELFCHECK_CHECK(!jsonlens::parse_json_text(inside, value, error));
  SELFCHECK_CHECK_CONTAINS(error, "NUL");
}

/* RFC 8259 forbids adding a byte order mark, and accepting one would mean that
   the text read back is not the text that was written. */
SELFCHECK_TEST(a_byte_order_mark_is_rejected) {
  const std::string text = std::string("\xef\xbb\xbf") + "{\"a\":1}";
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text(text, value, error));
  SELFCHECK_CHECK_CONTAINS(error, "byte order mark");
  SELFCHECK_CHECK_CONTAINS(error, "line 1 column 1");
}

/* Every sequence here is what a tool that guessed at a decoder in a different
   language would accept: an overlong form, a stray continuation byte, a
   truncated sequence and a code point that is not a character. */
SELFCHECK_TEST(a_byte_sequence_that_is_not_utf8_is_rejected) {
  const char *const cases[] = {
      "\"\xc0\x80\"",          /* an overlong NUL, two bytes for one character */
      "\"\x80\"",              /* a continuation byte with nothing to continue */
      "\"\xe2\x82\"",          /* a three byte sequence cut short at the quote */
      "\"\xed\xa0\x80\"",      /* a surrogate, which is not a character */
      "\"\xf4\x90\x80\x80\"",  /* U+110000, one past the last code point */
      "\"\xf5\x80\x80\x80\""}; /* a lead byte that no sequence starts with */
  for (const char *const text : cases) {
    jsonlens::JsonValue value;
    std::string error;
    SELFCHECK_CHECK_MSG(!jsonlens::parse_json_text(text, value, error), std::string(text));
    SELFCHECK_CHECK_CONTAINS(error, "UTF-8");
    SELFCHECK_CHECK(value.is_null());
  }
  /* The same bytes are fine outside a string, where they are just a byte that
     starts no value. */
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text("\xc3\xa9", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "not valid JSON");
}

SELFCHECK_TEST(valid_utf8_survives_the_round_trip) {
  const std::string characters = "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80";
  const std::string text = "{\"name\":\"" + characters + "\"}";
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text(text, value, error), error);
  const jsonlens::JsonValue *const name = value.member("name");
  SELFCHECK_CHECK(name != 0);
  std::string round_tripped;
  SELFCHECK_CHECK(name != 0 && name->as_string(round_tripped));
  SELFCHECK_CHECK_STREQ(characters, round_tripped);
  SELFCHECK_CHECK_STREQ(text, jsonlens::to_json_text(value, false));
}

/* A surrogate is half of a character, and half of a character is not text. The
   escape is written as a pair or it is an error; it is never three bytes that no
   UTF-8 decoder accepts. */
SELFCHECK_TEST(unpaired_surrogate_escapes_are_rejected) {
  const char *const cases[] = {
      "\"\\ud800\"",        /* a leading surrogate with nothing after it */
      "\"\\udc00\"",        /* a trailing surrogate with nothing before it */
      "\"\\ud800\\u0041\"", /* a leading surrogate followed by a character */
      "\"\\ud800\\ud800\"" /* two leading surrogates are still half a pair */};
  for (const char *const text : cases) {
    jsonlens::JsonValue value;
    std::string error;
    SELFCHECK_CHECK_MSG(!jsonlens::parse_json_text(text, value, error), std::string(text));
    SELFCHECK_CHECK_CONTAINS(error, "surrogate");
  }
}

SELFCHECK_TEST(a_pair_of_surrogate_escapes_is_one_character) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text("\"\\ud83d\\ude00\"", value, error), error);
  std::string text;
  SELFCHECK_CHECK(value.as_string(text));
  SELFCHECK_CHECK_STREQ(std::string("\xf0\x9f\x98\x80"), text);
  /* The escaped form and the literal one name the same character, so the
     document that comes back is written the literal way. */
  SELFCHECK_CHECK_STREQ(std::string("\"\xf0\x9f\x98\x80\""), jsonlens::to_json_text(value, false));
}

/* A \u0000 escape is a NUL byte in the middle of a string, which no C string can
   carry: the backend would truncate the value where the byte sits, and the text
   would read back shorter than it was written. */
SELFCHECK_TEST(a_nul_escape_is_rejected) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text("\"a\\u0000b\"", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "NUL");
  SELFCHECK_CHECK_CONTAINS(error, "\\u0000");
}

/* Every integer below 2^53 is exactly what a double holds, and at 2^53 the
   doubles are farther apart than one: 9007199254740992 and 9007199254740993 are
   the same double. The model stops calling a number an integer at that point,
   because a long long there would report a value the text never carried. */
SELFCHECK_TEST(an_integer_above_the_exact_range_stays_a_number) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text("9007199254740991", value, error), error);
  SELFCHECK_CHECK(value.is_integer());
  long long integer = 0;
  SELFCHECK_CHECK(value.as_integer(integer));
  SELFCHECK_CHECK(integer == 9007199254740991LL);

  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text("9007199254740992", value, error), error);
  SELFCHECK_CHECK(!value.is_integer());
  SELFCHECK_CHECK(value.is_number());

  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text("9007199254740993", value, error), error);
  SELFCHECK_CHECK(!value.is_integer());
  SELFCHECK_CHECK(value.is_number());
  double number = 0.0;
  SELFCHECK_CHECK(value.as_number(number));
  SELFCHECK_CHECK(number == 9007199254740992.0);
  /* It is written as the double it became, and the reader says so rather than
     claiming a precision it does not have. */
  SELFCHECK_CHECK_STREQ(std::string("9007199254740992"), jsonlens::to_json_text(value, false));

  /* A twenty digit integer is further past the limit, and it is not an integer
     of the model either. */
  SELFCHECK_CHECK_MSG(jsonlens::parse_json_text("{\"id\":123456789012345678901}", value, error),
                      error);
  const jsonlens::JsonValue *const id = value.member("id");
  SELFCHECK_CHECK(id != 0 && !id->is_integer());
  SELFCHECK_CHECK(id != 0 && id->is_number());
}

/* An exponent large enough to overflow a double becomes an infinity in the
   backend, and the backend prints an infinity as null. Losing 1e400 that way is
   the kind of silent loss this library exists to refuse. */
SELFCHECK_TEST(a_number_that_does_not_fit_the_backend_is_an_error) {
  jsonlens::JsonValue value;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::parse_json_text("{\"scale\":1e400}", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "scale");
  SELFCHECK_CHECK_CONTAINS(error, "out of range");
  SELFCHECK_CHECK(value.is_null());

  SELFCHECK_CHECK(!jsonlens::parse_json_text("1e400", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "the document");
  SELFCHECK_CHECK_CONTAINS(error, "out of range");

  /* The negative side is the same number of the backend. */
  SELFCHECK_CHECK(!jsonlens::parse_json_text("[-1e400]", value, error));
  SELFCHECK_CHECK_CONTAINS(error, "[0]");
}

/* A key is a C string to the backend just as a value is, and it is copied before
   the member is written, so a NUL in a key would truncate the key and not the
   member. */
SELFCHECK_TEST(writing_refuses_a_key_that_holds_a_NUL_byte) {
  const std::string key("a\0b", 3);
  jsonlens::JsonValue value = jsonlens::JsonValue::make_object();
  value.add(key, jsonlens::JsonValue::make_integer(1));
  std::string text;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::to_json_text(value, text, error, false));
  SELFCHECK_CHECK_CONTAINS(error, "NUL");
  SELFCHECK_CHECK(text.empty());
}
