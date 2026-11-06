/*
 * json_document_test.cpp - The C++ document facade over the value model.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * A JsonDocument is what an element uses instead of the C API of the backend: a
 * value, addressed by the same path syntax as the diagnostics of the library.
 *
 * These tests pin the two rules that make it safe to hand to an application. A
 * read is strict: a path that is not there, or a value of another kind, fails
 * and says so, where the legacy library returned a default value and said
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

/* A leading slash is ignored because the legacy paths carry one, and it is the
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

/* A document whose root is an array, which is how the legacy device tables were
   written: the whole document reads as items, and each item reads a field. */
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
