/*
 * json_index_test.cpp - The standalone scanner: path syntax, positions, keys.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * json_index.hpp is the part of the library that needs no backend at all, and it
 * is what a tool uses on its own to point at a line and a column. Two halves are
 * pinned here:
 *
 *   - the path syntax, which is a small language of its own: dot separated keys,
 *     array subscripts, a leading '/' that is ignored, a backslash that escapes
 *     the next character, and the shapes that are refused so that a path always
 *     names a walk that can be taken;
 *   - the index a scan produces: the kind and the one based line and column of
 *     every key and every value, which is the whole point of the scanner and the
 *     only thing a caller reports.
 *
 * ByteLess is pinned with the case that made it necessary: a key above 0x7f sorts
 * after the ASCII range on every platform, where a plain std::string comparison
 * on MSVC would put it first.
 */
#include <jsonlens/detail/byte_less.hpp>
#include <jsonlens/json_index.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "framework/selfcheck.hpp"

namespace {

/* Split a path and report how many steps it has, for the cases whose shape is
   the point. */
std::vector<jsonlens::JsonPathStep> steps_of(const std::string &path) {
  std::vector<jsonlens::JsonPathStep> steps;
  const bool ok = jsonlens::parse_json_path(path, steps);
  SELFCHECK_CHECK_MSG(ok, path);
  return steps;
}

bool path_is_valid(const std::string &path) {
  std::vector<jsonlens::JsonPathStep> steps;
  return jsonlens::parse_json_path(path, steps);
}

/* A text that scans, which a position test needs before it can ask anything. */
jsonlens::JsonIndex scanned(const std::string &text) {
  jsonlens::JsonIndex index;
  std::string error;
  const bool ok = jsonlens::JsonIndex::build(text, index, error);
  SELFCHECK_CHECK_MSG(ok, error);
  return index;
}

std::string position_text(const jsonlens::JsonPosition &position) {
  return jsonlens::decimal_text(position.line) + ":" + jsonlens::decimal_text(position.column);
}

} /* namespace */

/* ---------------------------------------------------------------------------
 * The path syntax.
 * ------------------------------------------------------------------------- */

SELFCHECK_TEST(a_path_is_split_into_keys_and_subscripts) {
  const std::vector<jsonlens::JsonPathStep> port = steps_of("device.port");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), port.size());
  SELFCHECK_CHECK(!port[0].is_index && port[0].key == "device");
  SELFCHECK_CHECK(!port[1].is_index && port[1].key == "port");

  const std::vector<jsonlens::JsonPathStep> pad = steps_of("pads[2].name");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(3), pad.size());
  SELFCHECK_CHECK(!pad[0].is_index && pad[0].key == "pads");
  SELFCHECK_CHECK(pad[1].is_index);
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), pad[1].index);
  SELFCHECK_CHECK(!pad[2].is_index && pad[2].key == "name");

  /* A subscript needs no key in front of it, and two in a row are two steps. */
  const std::vector<jsonlens::JsonPathStep> grid = steps_of("a[0][1]");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(3), grid.size());
  SELFCHECK_CHECK(!grid[0].is_index && grid[0].key == "a");
  SELFCHECK_CHECK(grid[1].is_index && grid[1].index == 0);
  SELFCHECK_CHECK(grid[2].is_index && grid[2].index == 1);
  const std::vector<jsonlens::JsonPathStep> root_item = steps_of("[0]");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), root_item.size());
  SELFCHECK_CHECK(root_item[0].is_index && root_item[0].index == 0);
}

/* An empty path is the document itself, and the leading slash of the familiar
   style is ignored and means nothing more. */
SELFCHECK_TEST(an_empty_path_has_no_step_and_a_leading_slash_is_only_ignored) {
  SELFCHECK_CHECK(steps_of("").empty());
  SELFCHECK_CHECK(steps_of("/").empty());
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), steps_of("/device.port").size());
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), steps_of("/a").size());
  /* A slash anywhere else is a character of a key, not a separator. */
  const std::vector<jsonlens::JsonPathStep> slashed = steps_of("device/port");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), slashed.size());
  SELFCHECK_CHECK_STREQ(std::string("device/port"), slashed[0].key);
}

/* A backslash makes the next character a character of the key, which is how a key
   that holds a dot is reached at all. */
SELFCHECK_TEST(a_backslash_escapes_one_character_of_a_key) {
  const std::vector<jsonlens::JsonPathStep> dotted = steps_of("a\\.b.c");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), dotted.size());
  SELFCHECK_CHECK_STREQ(std::string("a.b"), dotted[0].key);
  SELFCHECK_CHECK_STREQ(std::string("c"), dotted[1].key);

  const std::vector<jsonlens::JsonPathStep> brackets = steps_of("a\\[0\\]");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), brackets.size());
  SELFCHECK_CHECK(!brackets[0].is_index);
  SELFCHECK_CHECK_STREQ(std::string("a[0]"), brackets[0].key);

  const std::vector<jsonlens::JsonPathStep> backslash = steps_of("a\\\\b");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), backslash.size());
  SELFCHECK_CHECK_STREQ(std::string("a\\b"), backslash[0].key);

  /* A backslash that escapes nothing is a path that cannot be walked. */
  SELFCHECK_CHECK(!path_is_valid("a\\"));
  /* And a backslash is no way around the rule that a key may not be empty. */
  SELFCHECK_CHECK(!path_is_valid("a..b"));
}

/* A path that stands for something surprising is refused, so that a typo is a
   message and not a value nobody meant. */
SELFCHECK_TEST(a_path_that_cannot_be_walked_is_refused) {
  /* An empty key, which the empty path already stands for. */
  SELFCHECK_CHECK(!path_is_valid("a."));
  SELFCHECK_CHECK(!path_is_valid(".a"));
  SELFCHECK_CHECK(!path_is_valid("a..b"));
  /* A key right after a subscript needs a dot in front of it. */
  SELFCHECK_CHECK(!path_is_valid("a[0]b"));
  SELFCHECK_CHECK(!path_is_valid("a[0]b.c"));
  /* A subscript is digits between brackets, and nothing else. */
  SELFCHECK_CHECK(!path_is_valid("a["));
  SELFCHECK_CHECK(!path_is_valid("a[]"));
  SELFCHECK_CHECK(!path_is_valid("a[-1]"));
  SELFCHECK_CHECK(!path_is_valid("a[ 1]"));
  SELFCHECK_CHECK(!path_is_valid("a[1"));
  SELFCHECK_CHECK(!path_is_valid("a[1.0]"));
  /* What is valid, next to it, so that the list above is not passing because
     everything is refused. */
  SELFCHECK_CHECK(path_is_valid("a[0]"));
  SELFCHECK_CHECK(path_is_valid("a[0][1]"));
  SELFCHECK_CHECK(path_is_valid("a[0].b"));
  SELFCHECK_CHECK(path_is_valid("a.b[0]"));
}

/* A subscript is accumulated without a bound, so a path that names a position no
   document can hold is still a path: it is the document that says the position is
   out of range, and it says it with a count. */
SELFCHECK_TEST(a_large_subscript_is_a_path_of_its_own) {
  const std::vector<jsonlens::JsonPathStep> step = steps_of("a[18446744073709551615]");
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), step.size());
  SELFCHECK_CHECK(step.size() == 2 && step[1].is_index);
}

/* ---------------------------------------------------------------------------
 * Escaping a key into a path, and building a path from parts.
 * ------------------------------------------------------------------------- */

SELFCHECK_TEST(a_key_is_escaped_before_it_becomes_a_step) {
  SELFCHECK_CHECK_STREQ(std::string("plain"), jsonlens::escape_json_path_key("plain"));
  /* The four characters the syntax uses are the four that are escaped. */
  SELFCHECK_CHECK_STREQ(std::string("a\\.b"), jsonlens::escape_json_path_key("a.b"));
  SELFCHECK_CHECK_STREQ(std::string("a\\[b"), jsonlens::escape_json_path_key("a[b"));
  SELFCHECK_CHECK_STREQ(std::string("a\\]b"), jsonlens::escape_json_path_key("a]b"));
  SELFCHECK_CHECK_STREQ(std::string("a\\\\b"), jsonlens::escape_json_path_key("a\\b"));
  SELFCHECK_CHECK_STREQ(std::string(""), jsonlens::escape_json_path_key(""));
  /* A slash is not part of the syntax, so a key that holds one is left alone. */
  SELFCHECK_CHECK_STREQ(std::string("a/b"), jsonlens::escape_json_path_key("a/b"));
}

/* Escaping a key and splitting the result has to give the key back. The round trip
   is the reason both functions exist. */
SELFCHECK_TEST(an_escaped_key_splits_back_into_the_key_it_came_from) {
  const char *const keys[] = {"plain", "a.b", "a[0]", "back\\slash", "a.b[c]\\d", "sp ace"};
  for (const char *const key : keys) {
    const std::string path = jsonlens::escape_json_path_key(key);
    std::vector<jsonlens::JsonPathStep> steps;
    SELFCHECK_CHECK_MSG(jsonlens::parse_json_path(path, steps), path);
    SELFCHECK_CHECK_EQ(static_cast<std::size_t>(1), steps.size());
    if (steps.size() == 1) {
      SELFCHECK_CHECK_STREQ(std::string(key), steps[0].key);
    }
  }
}

SELFCHECK_TEST(a_key_and_a_subscript_are_appended_to_a_path) {
  /* An empty path is the root, so the first part carries no separator. */
  SELFCHECK_CHECK_STREQ(std::string("device"), jsonlens::append_json_path_key("", "device"));
  SELFCHECK_CHECK_STREQ(std::string("device.port"),
                        jsonlens::append_json_path_key("device", "port"));
  SELFCHECK_CHECK_STREQ(std::string("a.b.c"), jsonlens::append_json_path_key("a.b", "c"));
  /* A key that needs it is escaped on the way in. */
  SELFCHECK_CHECK_STREQ(std::string("device.na\\.me"),
                        jsonlens::append_json_path_key("device", "na.me"));

  SELFCHECK_CHECK_STREQ(std::string("[0]"), jsonlens::append_json_path_index("", 0));
  SELFCHECK_CHECK_STREQ(std::string("pads[2]"), jsonlens::append_json_path_index("pads", 2));
  SELFCHECK_CHECK_STREQ(std::string("a[0][1]"), jsonlens::append_json_path_index("a[0]", 1));
  SELFCHECK_CHECK_STREQ(std::string("a[10]"), jsonlens::append_json_path_index("a", 10));
}

/* ---------------------------------------------------------------------------
 * Positions and kinds of a scan.
 * ------------------------------------------------------------------------- */

SELFCHECK_TEST(a_scan_records_the_position_and_the_kind_of_a_value) {
  const jsonlens::JsonIndex index = scanned("{\"device\":{\"port\":1883,\"name\":\"uart0\"}}");

  /* The root starts at the first byte of the text and has no key. */
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::object, index.kind(""));
  SELFCHECK_CHECK_STREQ(std::string("1:1"), position_text(index.value_position("")));
  SELFCHECK_CHECK(!jsonlens::json_position_is_known(index.key_position("")));

  const jsonlens::JsonEntry device = index.entry("device");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::object, device.kind);
  SELFCHECK_CHECK_STREQ(std::string("1:2"), position_text(device.key));
  SELFCHECK_CHECK_STREQ(std::string("1:11"), position_text(device.value));

  /* A key is the start of the quote, a value the first byte after the colon and
     the space that follows it. */
  const jsonlens::JsonEntry port = index.entry("device.port");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::integer, port.kind);
  SELFCHECK_CHECK_STREQ(std::string("1:12"), position_text(port.key));
  SELFCHECK_CHECK_STREQ(std::string("1:19"), position_text(port.value));

  const jsonlens::JsonEntry name = index.entry("device.name");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::string, name.kind);
  SELFCHECK_CHECK_STREQ(std::string("1:31"), position_text(name.value));
}

/* The kind is what tells an integer from a number, which the backend cannot do:
   it holds both as one double. */
SELFCHECK_TEST(a_scan_tells_an_integer_from_a_number) {
  const jsonlens::JsonIndex index =
      scanned("{\"i\":1,\"n\":-2,\"d\":1.0,\"e\":1e3,\"E\":1E+3,\"neg\":-1.5}");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::integer, index.kind("i"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::integer, index.kind("n"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::number, index.kind("d"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::number, index.kind("e"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::number, index.kind("E"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::number, index.kind("neg"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::integer, index.kind("i"));
}

SELFCHECK_TEST(a_scan_records_the_kind_of_every_other_value) {
  const jsonlens::JsonIndex index =
      scanned("{\"t\":true,\"f\":false,\"z\":null,\"s\":\"x\",\"a\":[],\"o\":{}}");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::boolean, index.kind("t"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::boolean, index.kind("f"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::null_value, index.kind("z"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::string, index.kind("s"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::array, index.kind("a"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::object, index.kind("o"));
}

/* An array element has a value position but no key position, and a path that was
   never recorded is missing rather than a value of some kind. */
SELFCHECK_TEST(an_array_element_has_a_position_and_no_key) {
  const jsonlens::JsonIndex index = scanned("{\"pads\":[10,20,[30]]}");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::array, index.kind("pads"));

  const jsonlens::JsonEntry first = index.entry("pads[0]");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::integer, first.kind);
  SELFCHECK_CHECK(!jsonlens::json_position_is_known(first.key));
  SELFCHECK_CHECK_STREQ(std::string("1:10"), position_text(first.value));

  const jsonlens::JsonEntry second = index.entry("pads[1]");
  SELFCHECK_CHECK_STREQ(std::string("1:13"), position_text(second.value));

  const jsonlens::JsonEntry nested = index.entry("pads[2][0]");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::integer, nested.kind);
  SELFCHECK_CHECK_STREQ(std::string("1:17"), position_text(nested.value));
  /* The container itself is recorded too, so its own kind is known. */
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::array, index.kind("pads[2]"));
}

SELFCHECK_TEST(a_path_that_was_not_recorded_is_missing) {
  const jsonlens::JsonIndex index = scanned("{\"a\":1}");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::missing, index.kind("b"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::missing, index.kind("a.b"));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::missing, index.kind("a[0]"));
  const jsonlens::JsonEntry absent = index.entry("b");
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::missing, absent.kind);
  SELFCHECK_CHECK(!jsonlens::json_position_is_known(absent.value));
  SELFCHECK_CHECK(!jsonlens::json_position_is_known(absent.key));
  SELFCHECK_CHECK(!index.has("b"));
  SELFCHECK_CHECK(index.has("a"));
  SELFCHECK_CHECK(index.has(""));
  /* Every value of the text is recorded, and the root is one of them. */
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), index.size());
}

/* A line and a column are one based and count the bytes of the text, so a line
   break inside a string moves both. */
SELFCHECK_TEST(a_position_counts_lines_and_columns_from_one) {
  const jsonlens::JsonIndex index =
      scanned("{\n  \"a\": 1,\n  \"b\": [\n    \"text\",\n    true\n  ]\n}");
  SELFCHECK_CHECK_STREQ(std::string("2:3"), position_text(index.key_position("a")));
  SELFCHECK_CHECK_STREQ(std::string("2:8"), position_text(index.value_position("a")));
  SELFCHECK_CHECK_STREQ(std::string("3:3"), position_text(index.key_position("b")));
  SELFCHECK_CHECK_STREQ(std::string("4:5"), position_text(index.value_position("b[0]")));
  SELFCHECK_CHECK_STREQ(std::string("5:5"), position_text(index.value_position("b[1]")));
  SELFCHECK_CHECK_EQ(jsonlens::JsonTokenKind::boolean, index.kind("b[1]"));
}

/* A carriage return is whitespace like any other and is not a line break, so a
   text written on Windows and one written on Linux give the same line numbers. */
SELFCHECK_TEST(a_carriage_return_does_not_start_a_new_line) {
  const jsonlens::JsonIndex index = scanned("{\r\n  \"a\": 1\r\n}");
  SELFCHECK_CHECK_STREQ(std::string("2:3"), position_text(index.key_position("a")));
  SELFCHECK_CHECK_STREQ(std::string("2:8"), position_text(index.value_position("a")));
}

/* The message of a scan that failed carries the position, because "not valid" on
   its own is not a report a caller can act on. */
SELFCHECK_TEST(a_scan_that_fails_reports_a_line_and_a_column) {
  jsonlens::JsonIndex index;
  std::string error;
  SELFCHECK_CHECK(!jsonlens::JsonIndex::build("{\n  \"a\": 1,\n}", index, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("at line 3 column 1"));

  SELFCHECK_CHECK(!jsonlens::JsonIndex::build("", index, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("at line 1 column 1"));

  SELFCHECK_CHECK(!jsonlens::JsonIndex::build("{\"a\":1} trailing", index, error));
  SELFCHECK_CHECK_CONTAINS(error, std::string("unexpected trailing data"));
  SELFCHECK_CHECK_CONTAINS(error, std::string("at line 1 column 9"));
}

/* The duplicates the scanner reports are the names it saw twice, once each and in
   the order in which the repetition was found, and the list it builds for a
   message is sorted by byte. */
SELFCHECK_TEST(a_scan_reports_every_key_that_repeats_once) {
  const jsonlens::JsonIndex index = scanned("{\"b\":1,\"a\":1,\"b\":2,\"c\":1,\"a\":3,\"b\":4}");
  const std::vector<std::string> found = index.duplicate_keys();
  SELFCHECK_CHECK_EQ(static_cast<std::size_t>(2), found.size());
  SELFCHECK_CHECK(found.size() == 2 && found[0] == "b");
  SELFCHECK_CHECK(found.size() == 2 && found[1] == "a");
  SELFCHECK_CHECK_STREQ(std::string("a, b"), index.duplicated_key_list());

  const jsonlens::JsonIndex clean = scanned("{\"a\":1,\"b\":2}");
  SELFCHECK_CHECK(clean.duplicate_keys().empty());
  SELFCHECK_CHECK_STREQ(std::string(""), clean.duplicated_key_list());
}

/* ---------------------------------------------------------------------------
 * ByteLess, which is what makes the order of the message the same everywhere.
 * ------------------------------------------------------------------------- */

SELFCHECK_TEST(byte_less_orders_by_unsigned_byte_on_every_platform) {
  const jsonlens::ByteLess less;
  SELFCHECK_CHECK(less("a", "b"));
  SELFCHECK_CHECK(!less("b", "a"));
  SELFCHECK_CHECK(!less("a", "a"));
  /* A prefix sorts first. */
  SELFCHECK_CHECK(less("a", "ab"));
  SELFCHECK_CHECK(!less("ab", "a"));
  /* The case that made this type necessary: a byte above 0x7f is greater than
     any ASCII byte, where a signed char comparison would put it first. */
  SELFCHECK_CHECK(less("a", "\xc3\xa9"));
  SELFCHECK_CHECK(less("\x7f", "\x80"));
  SELFCHECK_CHECK(!less("\x80", "\x7f"));
  /* A longer string that has the other one as a prefix still sorts after it,
     whatever its last byte is. */
  SELFCHECK_CHECK(less("a", std::string("a\xff")));
}
