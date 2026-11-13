/*
 * json_document_test.cpp - The C++ document facade over the value model.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * A JsonDocument is what an application uses instead of the C API of the
 * backend: a value, addressed by the same path syntax as the diagnostics of the
 * library.
 *
 * These tests pin the two rules that make it safe to hand to an application. A
 * read is strict: a path that is not there, or a value of another kind, fails
 * and says so, where a lenient reader would return a default value and say
 * nothing. A write creates what the path needs and refuses everything else: it
 * never overwrites a number with an object, and it never pads an array with
 * values nobody asked for.
 */
#include <jsonlens/jsonlens.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "framework/selfcheck.hpp"

namespace {

/* A document with a nested object and an array of objects in it. */
jsonlens::JsonDocument loaded(const std::string &text) {
  jsonlens::JsonDocument document;
  std::string error;
  const bool ok = jsonlens::JsonDocument::parse(text, document, error);
  SELFCHECK_CHECK_MSG(ok, error);
  return document;
}

const char *const sample_text =
    "{\"device\":{\"port\":1883,\"name\":\"uart0\"},"
    "\"pads\":[{\"name\":\"in\",\"size\":4},{\"name\":\"out\",\"size\":2}]}";

/* A path of count keys, "a.a.a". */
std::string path_of(std::size_t count) {
  std::string path;
  for (std::size_t index = 0; index < count; ++index) {
    if (index > 0) {
      path += ".";
    }
    path += "a";
  }
  return path;
}

} /* namespace */

SELFCHECK_TEST(a_document_starts_as_an_empty_object) {
  const jsonlens::JsonDocument document;
  SELFCHECK_CHECK(document.root().is_object());
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(0), document.root().size());
  SELFCHECK_CHECK_STREQ(std::string("{}"), document.to_text(false));
  SELFCHECK_CHECK_STREQ(std::string("{}"), jsonlens::JsonDocument::object().to_text(false));
  SELFCHECK_CHECK_STREQ(std::string("[]"), jsonlens::JsonDocument::array().to_text(false));
}

SELFCHECK_TEST(a_document_reads_a_value_by_path) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  long long port = 0;
  std::string error;
  SELFCHECK_CHECK_MSG(document.get_integer("device.port", port, error), error);
  SELFCHECK_CHECK_EQ(static_cast<long long>(1883), port);
  std::string name;
  SELFCHECK_CHECK_MSG(document.get_string("device.name", name, error), error);
  SELFCHECK_CHECK_STREQ(std::string("uart0"), name);
  SELFCHECK_CHECK_MSG(document.get_string("pads[1].name", name, error), error);
  SELFCHECK_CHECK_STREQ(std::string("out"), name);
  long long size = 0;
  SELFCHECK_CHECK_MSG(document.get_integer("pads[0].size", size, error), error);
  SELFCHECK_CHECK_EQ(static_cast<long long>(4), size);
  SELFCHECK_CHECK(document.has("device.port"));
  SELFCHECK_CHECK(!document.has("device.rate"));
  const jsonlens::JsonValue *const pads = document.find("pads");
  SELFCHECK_CHECK(pads != 0 && pads->is_array());
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), pads->size());
  jsonlens::JsonValue value;
  SELFCHECK_CHECK_MSG(document.get_value("device", value, error), error);
  SELFCHECK_CHECK(value.is_object());
  /* A path with nothing in it is the document itself. */
  SELFCHECK_CHECK(document.has(""));
}

SELFCHECK_TEST(a_path_that_is_not_there_is_a_failure_that_names_it) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  long long port = 0;
  std::string error;
  SELFCHECK_CHECK(!document.get_integer("device.rate", port, error));
  SELFCHECK_CHECK_STREQ(std::string("device.rate: there is no value at this path"), error);
  SELFCHECK_CHECK(!document.get_integer("device.rate.deep", port, error));
  SELFCHECK_CHECK_STREQ(std::string("device.rate.deep: there is no value at this path"), error);
  SELFCHECK_CHECK(!document.get_integer("pads[9].name", port, error));
  SELFCHECK_CHECK_STREQ(std::string("pads[9].name: there is no value at this path"), error);
  SELFCHECK_CHECK(document.find("device.rate") == 0);
}

SELFCHECK_TEST(a_value_of_another_kind_is_a_failure_that_names_both_kinds) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  std::string error;
  std::string name;
  SELFCHECK_CHECK(!document.get_string("device.port", name, error));
  SELFCHECK_CHECK_STREQ(std::string("device.port: expected a string, found an integer"), error);
  long long port = 0;
  SELFCHECK_CHECK(!document.get_integer("device.name", port, error));
  SELFCHECK_CHECK_STREQ(std::string("device.name: expected an integer, found a string"), error);
  bool flag = false;
  SELFCHECK_CHECK(!document.get_bool("device", flag, error));
  SELFCHECK_CHECK_STREQ(std::string("device: expected a boolean, found an object"), error);
  SELFCHECK_CHECK(!document.get_string("pads", name, error));
  SELFCHECK_CHECK_STREQ(std::string("pads: expected a string, found an array"), error);
}

SELFCHECK_TEST(a_number_reads_as_a_number_however_it_was_written) {
  const jsonlens::JsonDocument document = loaded("{\"i\":12,\"d\":12.5,\"t\":true}");
  double number = 0.0;
  std::string error;
  SELFCHECK_CHECK_MSG(document.get_number("i", number, error), error);
  SELFCHECK_CHECK_NEAR(12.0, number, 1e-12);
  SELFCHECK_CHECK_MSG(document.get_number("d", number, error), error);
  SELFCHECK_CHECK_NEAR(12.5, number, 1e-12);
  /* The other way round is a failure: a number that was written with a fraction
     is not an integer, and pretending otherwise would hide a wrong datum. */
  long long integer = 0;
  SELFCHECK_CHECK(!document.get_integer("d", integer, error));
  SELFCHECK_CHECK_STREQ(std::string("d: expected an integer, found a number"), error);
  SELFCHECK_CHECK(document.get_integer("i", integer, error));
  bool flag = false;
  SELFCHECK_CHECK(document.get_bool("t", flag, error));
  SELFCHECK_CHECK_EQ(true, flag);
}

SELFCHECK_TEST(a_path_that_is_not_valid_is_a_failure) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  std::string error;
  long long number = 0;
  SELFCHECK_CHECK(!document.get_integer("device[", number, error));
  SELFCHECK_CHECK_CONTAINS(error, "the path is not valid");
  SELFCHECK_CHECK(!document.get_integer("device\\", number, error));
  SELFCHECK_CHECK_CONTAINS(error, "the path is not valid");
  SELFCHECK_CHECK(!document.has("device["));
  /* A key may not be empty, because the empty path is the document itself, and a
     key may not follow a subscript without a dot in front of it. */
  SELFCHECK_CHECK(!document.get_integer("device..port", number, error));
  SELFCHECK_CHECK_CONTAINS(error, "the path is not valid");
  SELFCHECK_CHECK(!document.get_integer("device.", number, error));
  SELFCHECK_CHECK_CONTAINS(error, "the path is not valid");
  SELFCHECK_CHECK(!document.get_integer(".port", number, error));
  SELFCHECK_CHECK_CONTAINS(error, "the path is not valid");
  SELFCHECK_CHECK(!document.get_integer("pads[0]name", number, error));
  SELFCHECK_CHECK_CONTAINS(error, "the path is not valid");
  jsonlens::JsonDocument writable;
  SELFCHECK_CHECK(!writable.set("pads[0", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_CONTAINS(error, "the path is not valid");
  SELFCHECK_CHECK_STREQ(std::string("{}"), writable.to_text(false));
}

/* A path cannot be deeper than a document this library can read: such a path
   could never name a value, and the walk it asks for is the walk a read costs. */
SELFCHECK_TEST(a_path_cannot_be_deeper_than_a_document_can_be) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  std::string error;
  long long number = 0;
  SELFCHECK_CHECK(
      !document.get_integer(path_of(jsonlens::json_nesting_limit() + 1), number, error));
  SELFCHECK_CHECK_CONTAINS(error, "more than the limit of");
  jsonlens::JsonDocument too_deep;
  SELFCHECK_CHECK(!too_deep.set(path_of(jsonlens::json_nesting_limit() + 1),
                                jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_CONTAINS(error, "more than the limit of");
  SELFCHECK_CHECK_STREQ(std::string("{}"), too_deep.to_text(false));

  /* A path at the limit is a path like any other. */
  const std::string deepest = path_of(jsonlens::json_nesting_limit());
  jsonlens::JsonDocument deep;
  SELFCHECK_CHECK_MSG(deep.set(deepest, jsonlens::JsonValue::make_integer(1), error), error);
  SELFCHECK_CHECK(deep.has(deepest));
  SELFCHECK_CHECK_MSG(deep.get_integer(deepest, number, error), error);
  SELFCHECK_CHECK_EQ(static_cast<long long>(1), number);
}

/* A leading slash is ignored because familiar paths carry one, and it is the
   only part of that syntax this library accepts. A slash inside the path is a
   slash in a key name, which is what a key named "device/port" is. */
SELFCHECK_TEST(a_leading_slash_is_ignored_and_nothing_more) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  long long port = 0;
  std::string error;
  SELFCHECK_CHECK_MSG(document.get_integer("/device.port", port, error), error);
  SELFCHECK_CHECK_EQ(static_cast<long long>(1883), port);
  SELFCHECK_CHECK(!document.has("/device/port"));
  jsonlens::JsonDocument slashed;
  SELFCHECK_CHECK(slashed.set("/device/port", jsonlens::JsonValue::make_integer(1883), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"device/port\":1883}"), slashed.to_text(false));
}

SELFCHECK_TEST(a_write_adds_a_key_and_keeps_the_place_of_one_that_is_there) {
  jsonlens::JsonDocument document = loaded("{\"first\":1,\"second\":2,\"third\":3}");
  std::string error;
  SELFCHECK_CHECK(document.set("second", jsonlens::JsonValue::make_integer(20), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"first\":1,\"second\":20,\"third\":3}"),
                        document.to_text(false));
  SELFCHECK_CHECK(document.set("fourth", jsonlens::JsonValue::make_integer(4), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"first\":1,\"second\":20,\"third\":3,\"fourth\":4}"),
                        document.to_text(false));
  /* A key is a string, so a short one and a long one are different keys. */
  SELFCHECK_CHECK(document.set("sec", jsonlens::JsonValue::make_integer(5), error));
  SELFCHECK_CHECK(document.has("sec") && document.has("second"));
}

SELFCHECK_TEST(a_write_creates_the_steps_that_are_missing) {
  jsonlens::JsonDocument document;
  std::string error;
  SELFCHECK_CHECK(document.set("a.b.c", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":{\"b\":{\"c\":1}}}"), document.to_text(false));
  /* The step that is created follows the step that comes next: a key needs an
     object and an index needs an array. */
  jsonlens::JsonDocument arrays;
  SELFCHECK_CHECK(arrays.set("pads[0].name", jsonlens::JsonValue::make_string("in"), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"pads\":[{\"name\":\"in\"}]}"), arrays.to_text(false));
  SELFCHECK_CHECK(arrays.set("pads[1].name", jsonlens::JsonValue::make_string("out"), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"pads\":[{\"name\":\"in\"},{\"name\":\"out\"}]}"),
                        arrays.to_text(false));
}

SELFCHECK_TEST(a_write_grows_an_array_one_item_at_a_time) {
  jsonlens::JsonDocument document;
  std::string error;
  SELFCHECK_CHECK(document.set("a[0]", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK(document.set("a[1]", jsonlens::JsonValue::make_integer(2), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":[1,2]}"), document.to_text(false));
  /* An item keeps its place when it is written again. */
  SELFCHECK_CHECK(document.set("a[0]", jsonlens::JsonValue::make_integer(10), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":[10,2]}"), document.to_text(false));
  /* The position one past the end appends, and further than that is a failure
     rather than a run of values nobody asked for. */
  SELFCHECK_CHECK(!document.set("a[3]", jsonlens::JsonValue::make_integer(4), error));
  SELFCHECK_CHECK_STREQ(std::string("a: the array holds 2 item(s), so a[3] is out of range"),
                        error);
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":[10,2]}"), document.to_text(false));
}

SELFCHECK_TEST(a_write_refuses_to_go_through_a_value_that_cannot_hold_the_path) {
  jsonlens::JsonDocument document = loaded("{\"a\":5,\"b\":[],\"c\":\"text\"}");
  std::string error;
  SELFCHECK_CHECK(!document.set("a.b", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("a: expected an object, found an integer"), error);
  SELFCHECK_CHECK(!document.set("b.key", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("b: expected an object, found an array"), error);
  SELFCHECK_CHECK(!document.set("c[0]", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("c: expected an array, found a string"), error);
  /* Nothing was changed by any of the three. */
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":5,\"b\":[],\"c\":\"text\"}"), document.to_text(false));
  /* The document itself has to be a container for a path to go through it. */
  const jsonlens::JsonDocument scalar =
      jsonlens::JsonDocument::of(jsonlens::JsonValue::make_integer(5));
  jsonlens::JsonDocument writable = scalar;
  SELFCHECK_CHECK(!writable.set("a", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("the document: expected an object, found an integer"), error);
}

/* The step that is created is chosen by the step that comes next, at every depth
   of a path that starts from nothing: a key needs an object, a subscript needs an
   array, and the two can alternate. */
SELFCHECK_TEST(a_write_creates_the_step_that_the_next_one_needs) {
  jsonlens::JsonDocument document;
  std::string error;
  SELFCHECK_CHECK_MSG(document.set("a[0].b", jsonlens::JsonValue::make_integer(1), error), error);
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":[{\"b\":1}]}"), document.to_text(false));

  /* Two subscripts in a row: the step that is created is an array because the
     next step is a subscript, and the one below it follows the same rule. The
     walk can only append, so the first item of a fresh array is [0] and never a
     position further on. */
  jsonlens::JsonDocument deeper;
  SELFCHECK_CHECK_MSG(deeper.set("a[0][0].c", jsonlens::JsonValue::make_integer(2), error), error);
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":[[{\"c\":2}]]}"), deeper.to_text(false));
  /* A position past the end of the array that was just created is still out of
     range, which is what keeps a write from padding a document with values
     nobody asked for. */
  jsonlens::JsonDocument refused;
  SELFCHECK_CHECK(!refused.set("a[0][1].c", jsonlens::JsonValue::make_integer(2), error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("out of range"));

  jsonlens::JsonDocument object_then_index;
  SELFCHECK_CHECK_MSG(object_then_index.set("a.b[0]", jsonlens::JsonValue::make_string("x"), error),
                      error);
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":{\"b\":[\"x\"]}}"), object_then_index.to_text(false));
}

/* A position past the end of an array is a failure whatever the array holds, and
   the count in the message is what the caller needs to see why. */
SELFCHECK_TEST(a_write_past_the_end_of_an_array_is_out_of_range) {
  jsonlens::JsonDocument empty;
  std::string error;
  SELFCHECK_CHECK(!empty.set("a[5]", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("a: the array holds 0 item(s), so a[5] is out of range"),
                        error);
  SELFCHECK_CHECK_STREQ(std::string("{}"), empty.to_text(false));

  /* The same at the end of a longer path, where the array is not the root of the
     walk. */
  jsonlens::JsonDocument document = loaded("{\"pads\":[{\"name\":\"in\"}]}");
  SELFCHECK_CHECK(!document.set("pads[2].name", jsonlens::JsonValue::make_string("out"), error));
  SELFCHECK_CHECK_STREQ(std::string("pads: the array holds 1 item(s), so pads[2] is out of range"),
                        error);
  SELFCHECK_CHECK_STREQ(std::string("{\"pads\":[{\"name\":\"in\"}]}"), document.to_text(false));
  /* One past the end appends, which is the boundary the rule above is measured
     against. */
  SELFCHECK_CHECK_MSG(document.set("pads[1].name", jsonlens::JsonValue::make_string("out"), error),
                      error);
  SELFCHECK_CHECK_STREQ(std::string("{\"pads\":[{\"name\":\"in\"},{\"name\":\"out\"}]}"),
                        document.to_text(false));
}

/* A write that fails at a level below the root leaves the document exactly as it
   was, including the copies of the containers the walk already made: nothing is
   put back by halves. */
SELFCHECK_TEST(a_write_that_fails_deep_inside_changes_nothing) {
  const std::string before = "{\"a\":{\"b\":5},\"c\":[{\"d\":1}]}";
  jsonlens::JsonDocument document = loaded(before);
  std::string error;
  SELFCHECK_CHECK(!document.set("a.b.c", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("a.b: expected an object, found an integer"), error);
  SELFCHECK_CHECK_STREQ(before, document.to_text(false));

  SELFCHECK_CHECK(!document.set("c[0].d.e", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("c[0].d: expected an object, found an integer"), error);
  SELFCHECK_CHECK_STREQ(before, document.to_text(false));

  /* A null in the middle is replaced by what the next step needs, even when that
     step is a subscript. The array that replaces it starts empty, so only [0]
     can be written into it in one call. */
  jsonlens::JsonDocument with_nulls = loaded("{\"a\":null,\"b\":null}");
  SELFCHECK_CHECK_MSG(with_nulls.set("a[0].b", jsonlens::JsonValue::make_integer(2), error), error);
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":[{\"b\":2}],\"b\":null}"), with_nulls.to_text(false));
  /* A null under a subscript is replaced the same way, and the rest of the
     document is untouched. */
  jsonlens::JsonDocument under_index = loaded("{\"a\":[null,{\"c\":3}]}");
  SELFCHECK_CHECK_MSG(under_index.set("a[0].b", jsonlens::JsonValue::make_integer(4), error),
                      error);
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":[{\"b\":4},{\"c\":3}]}"), under_index.to_text(false));
}

/* Every read clears the reason of the one before it, including a read that fails,
   so last_error() is never the reason of something else. */
SELFCHECK_TEST(a_failed_read_keeps_its_own_reason_and_a_read_clears_it) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  std::string error;
  long long port = 0;
  SELFCHECK_CHECK(!document.get_integer("device.missing", port, error));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("device.missing"));

  /* Another failure replaces the reason rather than adding to it. */
  std::string name;
  SELFCHECK_CHECK(!document.get_string("device.port", name, error));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("expected a string"));
  SELFCHECK_CHECK(document.last_error().find("missing") == std::string::npos);

  /* A success clears it, including a read that copies the whole document. */
  SELFCHECK_CHECK_MSG(document.get_integer("device.port", port, error), error);
  SELFCHECK_CHECK(document.last_error().empty());
  jsonlens::JsonValue device;
  SELFCHECK_CHECK_MSG(document.get_value("device", device, error), error);
  SELFCHECK_CHECK(document.last_error().empty());

  /* has() and find() are lookups and not reads: they leave the reason alone,
     because a caller that probes for an optional key must not lose the reason of
     the read that fell back before it. */
  SELFCHECK_CHECK(!document.get_integer("device.missing", port, error));
  SELFCHECK_CHECK(!document.last_error().empty());
  SELFCHECK_CHECK(document.has("device.port"));
  SELFCHECK_CHECK(!document.last_error().empty());
  SELFCHECK_CHECK(document.find("device.missing") == 0);
  SELFCHECK_CHECK(!document.last_error().empty());
}

SELFCHECK_TEST(a_write_replaces_a_null_in_the_middle) {
  jsonlens::JsonDocument document = loaded("{\"a\":null,\"b\":null,\"c\":9}");
  std::string error;
  /* A null carries nothing, so it is what the step needs, not a value that
     blocks the path. */
  SELFCHECK_CHECK(document.set("a.x", jsonlens::JsonValue::make_integer(1), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":{\"x\":1},\"b\":null,\"c\":9}"),
                        document.to_text(false));
  SELFCHECK_CHECK(document.set("b[0]", jsonlens::JsonValue::make_integer(2), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":{\"x\":1},\"b\":[2],\"c\":9}"),
                        document.to_text(false));
  /* Writing the null itself is a write like any other. */
  SELFCHECK_CHECK(document.set("b", jsonlens::JsonValue::make_null(), error));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":{\"x\":1},\"b\":null,\"c\":9}"),
                        document.to_text(false));
}

SELFCHECK_TEST(an_empty_path_is_the_whole_document) {
  jsonlens::JsonDocument document = loaded(sample_text);
  std::string error;
  SELFCHECK_CHECK(document.set("", jsonlens::JsonValue::make_integer(5), error));
  SELFCHECK_CHECK_STREQ(std::string("5"), document.to_text(false));
  long long number = 0;
  SELFCHECK_CHECK_MSG(document.get_integer("", number, error), error);
  SELFCHECK_CHECK_EQ(static_cast<long long>(5), number);
}

/* The model is what a document is built from, so the two writes it needs beyond
   add() are pinned here: one that says whether the value went in, and one that
   can change an item of an array. */
SELFCHECK_TEST(the_model_reports_a_write_that_could_not_be_placed) {
  jsonlens::JsonValue object = jsonlens::JsonValue::make_object();
  SELFCHECK_CHECK(object.try_add("a", jsonlens::JsonValue::make_integer(1)));
  SELFCHECK_CHECK(object.try_add("b", jsonlens::JsonValue::make_integer(2)));
  SELFCHECK_CHECK(object.try_add("a", jsonlens::JsonValue::make_integer(10)));
  SELFCHECK_CHECK_STREQ(std::string("{\"a\":10,\"b\":2}"), jsonlens::to_json_text(object, false));
  SELFCHECK_CHECK(!object.try_add(jsonlens::JsonValue::make_integer(3)));

  jsonlens::JsonValue array = jsonlens::JsonValue::make_array();
  SELFCHECK_CHECK(array.try_add(jsonlens::JsonValue::make_integer(1)));
  SELFCHECK_CHECK(array.try_add(jsonlens::JsonValue::make_integer(2)));
  SELFCHECK_CHECK(!array.try_add("key", jsonlens::JsonValue::make_integer(3)));
  SELFCHECK_CHECK(array.set_at(0, jsonlens::JsonValue::make_integer(10)));
  SELFCHECK_CHECK(array.set_at(2, jsonlens::JsonValue::make_integer(3)));
  SELFCHECK_CHECK(!array.set_at(4, jsonlens::JsonValue::make_integer(4)));
  SELFCHECK_CHECK_STREQ(std::string("[10,2,3]"), jsonlens::to_json_text(array, false));

  const jsonlens::JsonValue scalar = jsonlens::JsonValue::make_integer(7);
  jsonlens::JsonValue copy = scalar;
  SELFCHECK_CHECK(!copy.try_add(jsonlens::JsonValue::make_integer(1)));
  SELFCHECK_CHECK(!copy.set_at(0, jsonlens::JsonValue::make_integer(1)));
  SELFCHECK_CHECK_STREQ(std::string("7"), jsonlens::to_json_text(copy, false));
}

/* A parameter that has a default reads with one, and the reason it did is kept:
   a missing key and a key of another kind both take the fallback, and the caller
   can still tell which one happened. */
SELFCHECK_TEST(a_read_with_a_default_keeps_the_reason) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  SELFCHECK_CHECK_STREQ(std::string("fallback"),
                        document.get_string_or("device.missing", "fallback"));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("device.missing"));

  SELFCHECK_CHECK_EQ(static_cast<long long>(1883), document.get_integer_or("device.port", 0));
  SELFCHECK_CHECK(document.last_error().empty());

  SELFCHECK_CHECK_EQ(static_cast<long long>(7), document.get_integer_or("device.name", 7));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("expected an integer"));

  SELFCHECK_CHECK_NEAR(1883.0, document.get_number_or("device.port", 0.5), 1e-12);
  SELFCHECK_CHECK_NEAR(0.5, document.get_number_or("device.name", 0.5), 1e-12);
  SELFCHECK_CHECK(document.get_bool_or("device.port", true));
}

/* A document whose root is an array, which is a common shape for a table of
   records: the whole document reads as items, and each item reads a field. */
SELFCHECK_TEST(an_array_reads_as_items_including_the_root) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  std::string error;
  std::vector<jsonlens::JsonValue> items;
  SELFCHECK_CHECK_MSG(document.get_array("pads", items, error), error);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), items.size());
  SELFCHECK_CHECK_STREQ(std::string("in"), items[0].member("name")->get_string_or(""));
  SELFCHECK_CHECK_EQ(static_cast<long long>(2), items[1].member("size")->get_integer_or(0));

  const jsonlens::JsonDocument table = loaded("[{\"name\":\"a\"},{\"name\":\"b\"}]");
  items.clear();
  SELFCHECK_CHECK_MSG(table.get_array("/", items, error), error);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), items.size());
  SELFCHECK_CHECK_STREQ(std::string("b"), items[1].member("name")->get_string_or(""));

  std::vector<jsonlens::JsonDocument> entries;
  SELFCHECK_CHECK_MSG(table.get_documents("/", entries, error), error);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), entries.size());
  SELFCHECK_CHECK_STREQ(std::string("a"), entries[0].get_string_or("/name", ""));
}

/* An array read fails for the three reasons any read fails - no such path, a
   value of another kind, an invalid path - and it leaves the vector it was given
   alone, so a caller cannot take a half filled list for a table. */
SELFCHECK_TEST(an_array_read_fails_and_leaves_the_items_alone) {
  const jsonlens::JsonDocument document = loaded(sample_text);
  std::string error;

  std::vector<jsonlens::JsonValue> items(1, jsonlens::JsonValue::make_string("before"));
  SELFCHECK_CHECK(!document.get_array("device.missing", items, error));
  SELFCHECK_CHECK_STREQ(std::string("device.missing: there is no value at this path"), error);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), items.size());
  SELFCHECK_CHECK(items.size() == 1 && items[0].is_string());

  SELFCHECK_CHECK(!document.get_array("device", items, error));
  SELFCHECK_CHECK_STREQ(std::string("device: expected an array, found an object"), error);
  SELFCHECK_CHECK(!document.get_array("device.port", items, error));
  SELFCHECK_CHECK_STREQ(std::string("device.port: expected an array, found an integer"), error);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), items.size());

  SELFCHECK_CHECK(!document.get_array("pads[", items, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("the path is not valid"));
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), items.size());

  const jsonlens::JsonDocument with_empty = loaded("{\"a\":[]}");
  SELFCHECK_CHECK_MSG(with_empty.get_array("a", items, error), error);
  SELFCHECK_CHECK(items.empty());
}

/* An array of scalars fails on the first item that has another kind, names it and
   keeps what the caller had, the same rule for a number and for a string as for
   an integer. */
SELFCHECK_TEST(an_array_of_numbers_or_strings_fails_on_a_wrong_item) {
  const jsonlens::JsonDocument document =
      loaded("{\"scales\":[0.5,\"half\"],\"names\":[\"a\",1],\"ints\":[],\"mixed\":[1,2.5]}");
  std::string error;

  std::vector<double> scales(1, 42.0);
  const bool scales_ok = document.get_number_array("scales", scales, error);
  SELFCHECK_CHECK(!scales_ok);
  if (!scales_ok) {
    SELFCHECK_CHECK_STREQ(std::string("scales[1]: expected a number, found a string"), error);
    SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("scales[1]"));
    SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), scales.size());
    SELFCHECK_CHECK_NEAR(42.0, scales[0], 1e-12);
  }

  std::vector<std::string> names(1, "before");
  const bool names_ok = document.get_string_array("names", names, error);
  SELFCHECK_CHECK(!names_ok);
  if (!names_ok) {
    SELFCHECK_CHECK_STREQ(std::string("names[1]: expected a string, found an integer"), error);
    SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), names.size());
    SELFCHECK_CHECK_STREQ(std::string("before"), names[0]);
  }

  /* An integer is a number, so a mixed array of both reads as numbers, and an
     empty array reads as an empty list. */
  std::vector<double> mixed;
  SELFCHECK_CHECK_MSG(document.get_number_array("mixed", mixed, error), error);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), mixed.size());
  SELFCHECK_CHECK_NEAR(2.5, mixed[1], 1e-12);
  std::vector<long long> ints(1, 7);
  SELFCHECK_CHECK_MSG(document.get_int_array("ints", ints, error), error);
  SELFCHECK_CHECK(ints.empty());
  SELFCHECK_CHECK(document.last_error().empty());
}

/* An array of documents is not checked for its shape: an item of any kind is
   wrapped as it is, and a field of it fails on that item alone, where the scalar
   arrays above, which do expect a kind, refuse such an array. This is what makes a
   table that is sometimes a name and sometimes a record readable. */
SELFCHECK_TEST(an_array_of_documents_takes_whatever_kind_an_item_is) {
  const jsonlens::JsonDocument document =
      loaded("{\"items\":[{\"name\":\"a\"},1,\"x\",null,[],true]}");
  std::vector<jsonlens::JsonDocument> items;
  std::string error;

  SELFCHECK_CHECK_MSG(document.get_documents("items", items, error), error);
  SELFCHECK_CHECK_STREQ(std::string(""), error);
  SELFCHECK_CHECK(document.last_error().empty());
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(6), items.size());
  if (items.size() != 6) {
    return;
  }
  SELFCHECK_CHECK(items[0].root().is_object());
  SELFCHECK_CHECK(items[1].root().is_integer());
  SELFCHECK_CHECK(items[2].root().is_string());
  SELFCHECK_CHECK(items[3].root().is_null());
  SELFCHECK_CHECK(items[4].root().is_array());
  SELFCHECK_CHECK(items[5].root().is_bool());

  /* The item that is an object reads its field by path like any document, and an
     item that is not reads the failure of that one item and nothing more. */
  std::string name;
  SELFCHECK_CHECK_MSG(items[0].get_string("name", name, error), error);
  SELFCHECK_CHECK_STREQ(std::string("a"), name);
  SELFCHECK_CHECK(!items[1].get_string("name", name, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("there is no value at this path"));
  /* A scalar item reads a member of itself by the empty path. */
  long long whole = 0;
  SELFCHECK_CHECK_MSG(items[1].get_integer("", whole, error), error);
  SELFCHECK_CHECK_EQ(static_cast<long long>(1), whole);

  /* The same three failures as any other array read. */
  std::vector<jsonlens::JsonDocument> kept(
      1, jsonlens::JsonDocument::of(jsonlens::JsonValue::make_string("before")));
  SELFCHECK_CHECK(!document.get_documents("items[2]", kept, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("expected an array"));
  SELFCHECK_CHECK_CONTAINS(error, std::string("found a string"));
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), kept.size());
  SELFCHECK_CHECK(!document.get_documents("items[0].missing", kept, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("there is no value at this path"));
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), kept.size());

  const jsonlens::JsonDocument empty = loaded("{\"items\":[]}");
  SELFCHECK_CHECK_MSG(empty.get_documents("items", items, error), error);
  SELFCHECK_CHECK(items.empty());
}

/* Every read with a default keeps the reason it fell back, which is the whole
   point of the separate name: a caller that reports its defaults can tell a
   missing key from a key of another kind. */
SELFCHECK_TEST(every_read_with_a_default_keeps_its_reason) {
  const jsonlens::JsonDocument document = loaded("{\"flag\":true,\"count\":3,\"scale\":0.5}");
  SELFCHECK_CHECK(document.get_bool_or("flag", false));
  SELFCHECK_CHECK(document.last_error().empty());
  SELFCHECK_CHECK_EQ(static_cast<long long>(3), document.get_integer_or("count", 9));
  SELFCHECK_CHECK(document.last_error().empty());
  SELFCHECK_CHECK_NEAR(0.5, document.get_number_or("scale", 9.0), 1e-12);
  SELFCHECK_CHECK(document.last_error().empty());

  /* Missing: the fallback, and the reason names the path. */
  SELFCHECK_CHECK(document.get_bool_or("missing", true));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("missing"));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("no value at this path"));
  SELFCHECK_CHECK_EQ(static_cast<long long>(9), document.get_integer_or("missing", 9));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("no value at this path"));
  SELFCHECK_CHECK_NEAR(9.0, document.get_number_or("missing", 9.0), 1e-12);
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("no value at this path"));

  /* Another kind: the fallback, and the reason names both kinds. */
  SELFCHECK_CHECK(document.get_bool_or("count", true));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("expected a boolean"));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("found an integer"));
  SELFCHECK_CHECK_EQ(static_cast<long long>(9), document.get_integer_or("scale", 9));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("expected an integer"));
  SELFCHECK_CHECK_NEAR(9.0, document.get_number_or("flag", 9.0), 1e-12);
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("expected a number"));

  /* A read that succeeds clears the reason of the one before it. */
  SELFCHECK_CHECK_EQ(static_cast<long long>(3), document.get_integer_or("count", 9));
  SELFCHECK_CHECK(document.last_error().empty());
  SELFCHECK_CHECK(document.get_string_or("count", "fallback") == "fallback");
  SELFCHECK_CHECK(!document.last_error().empty());
}

SELFCHECK_TEST(an_array_of_scalars_reads_one_item_at_a_time) {
  const jsonlens::JsonDocument document =
      loaded("{\"ids\":[1,2,3],\"scales\":[0.5,1],\"names\":[\"a\",\"b\"]}");
  std::string error;
  std::vector<long long> ids;
  SELFCHECK_CHECK_MSG(document.get_int_array("ids", ids, error), error);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(3), ids.size());
  SELFCHECK_CHECK_EQ(static_cast<long long>(2), ids[1]);

  std::vector<double> scales;
  SELFCHECK_CHECK_MSG(document.get_number_array("scales", scales, error), error);
  SELFCHECK_CHECK_NEAR(1.0, scales[1], 1e-12);

  std::vector<std::string> names;
  SELFCHECK_CHECK_MSG(document.get_string_array("names", names, error), error);
  SELFCHECK_CHECK_STREQ(std::string("a"), names[0]);
}

/* One wrong item fails the whole read and names the item, because half a table
   is worse than no table: the caller reports the path and reads it again. */
SELFCHECK_TEST(an_array_with_one_wrong_item_names_it_and_changes_nothing) {
  const jsonlens::JsonDocument document = loaded("{\"ids\":[1,\"two\"]}");
  std::vector<long long> ids(1, 42);
  std::string error;
  SELFCHECK_CHECK(!document.get_int_array("ids", ids, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("ids[1]"));
  SELFCHECK_CHECK_CONTAINS(error, std::string("expected an integer"));
  SELFCHECK_CHECK_CONTAINS(document.last_error(), std::string("ids[1]"));
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), ids.size());
  SELFCHECK_CHECK_EQ(static_cast<long long>(42), ids[0]);
}
