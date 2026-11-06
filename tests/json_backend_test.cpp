/*
 * json_backend_test.cpp - Golden tests for the vendored cJSON backend.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * These tests pin the serialization behaviour that the backend exposes to
 * callers, so that replacing the backend is a conscious decision instead of an
 * accident:
 *
 *   - number text is produced with "%.15g" and only falls back to "%.17g"
 *     when that text does not round trip,
 *   - NaN and the infinities are written as null,
 *   - object keys keep their insertion order,
 *   - the formatted printer indents objects with a single TAB per level and
 *     puts a TAB between a key and its value, while arrays stay on one line.
 */
#include <jsonlens/detail/cjson.hpp>

#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

#include "framework/selfcheck.hpp"

namespace {

/* Print an item with the formatted printer and take ownership of the text. */
std::string print_formatted(const cJSON *item) {
  char *text = cJSON_Print(item);
  const std::string result = (text != NULL) ? text : "<cJSON_Print returned NULL>";
  cJSON_free(text);
  return result;
}

/* Print an item without any whitespace and take ownership of the text. */
std::string print_compact(const cJSON *item) {
  char *text = cJSON_PrintUnformatted(item);
  const std::string result = (text != NULL) ? text : "<cJSON_PrintUnformatted returned NULL>";
  cJSON_free(text);
  return result;
}

/* Serialize a single number exactly the way a JSON tree would serialize it. */
std::string number_text(double value) {
  cJSON *item = cJSON_CreateNumber(value);
  const std::string text = print_formatted(item);
  cJSON_Delete(item);
  return text;
}

/* Build a two member object and serialize it without whitespace. */
std::string compact_order_text() {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "zebra", cJSON_CreateNumber(1.0));
  cJSON_AddItemToObject(root, "alpha", cJSON_CreateNumber(2.0));
  const std::string result = print_compact(root);
  cJSON_Delete(root);
  return result;
}

} /* namespace */

SELFCHECK_TEST(json_backend_is_the_pinned_release) {
  SELFCHECK_CHECK_STREQ(std::string("1.7.10"), std::string(JSONLENS_CJSON_VERSION));
  SELFCHECK_CHECK_STREQ(std::string("1.7.10"), std::string(cJSON_Version()));
  SELFCHECK_CHECK_EQ(1, CJSON_VERSION_MAJOR);
  SELFCHECK_CHECK_EQ(7, CJSON_VERSION_MINOR);
  SELFCHECK_CHECK_EQ(10, CJSON_VERSION_PATCH);
  SELFCHECK_CHECK_EQ(1000, static_cast<int>(CJSON_NESTING_LIMIT));
}

SELFCHECK_TEST(json_backend_prints_integers_without_a_fraction) {
  SELFCHECK_CHECK_STREQ(std::string("12"), number_text(12.0));
  SELFCHECK_CHECK_STREQ(std::string("0"), number_text(0.0));
  SELFCHECK_CHECK_STREQ(std::string("-0"), number_text(-0.0));
  SELFCHECK_CHECK_STREQ(std::string("1024"), number_text(1024.0));
  SELFCHECK_CHECK_STREQ(std::string("-42"), number_text(-42.0));
}

SELFCHECK_TEST(json_backend_prints_large_magnitudes_in_exponent_form) {
  SELFCHECK_CHECK_STREQ(std::string("1e+300"), number_text(1e300));
  SELFCHECK_CHECK_STREQ(std::string("1e-07"), number_text(1e-7));
}

SELFCHECK_TEST(json_backend_round_trips_values_that_short_text_loses) {
  /* 2^53 needs 16 significant digits, so the %.15g form is rejected and the
     %.17g form is used instead. */
  SELFCHECK_CHECK_STREQ(std::string("9007199254740992"), number_text(9007199254740992.0));

  /* A float widened to double is not a short decimal either: the %.15g form
     would give "12.34", which no longer compares equal to the stored value.
     The value is parsed instead of written as 12.34f, because 32 bit x86 can
     evaluate the literal in an x87 register and keep more than float
     precision, which would silently turn the test into the double case. */
  const double widened = static_cast<double>(std::strtof("12.34", NULL));
  SELFCHECK_CHECK_STREQ(std::string("12.340000152587891"), number_text(widened));

  /* And neither is one third. */
  SELFCHECK_CHECK_STREQ(std::string("0.33333333333333331"), number_text(1.0 / 3.0));
}

SELFCHECK_TEST(json_backend_writes_non_finite_numbers_as_null) {
  const double nan_value = std::numeric_limits<double>::quiet_NaN();
  const double infinity = std::numeric_limits<double>::infinity();
  SELFCHECK_CHECK_STREQ(std::string("null"), number_text(nan_value));
  SELFCHECK_CHECK_STREQ(std::string("null"), number_text(infinity));
  SELFCHECK_CHECK_STREQ(std::string("null"), number_text(-infinity));
}

SELFCHECK_TEST(json_backend_keeps_object_keys_in_insertion_order) {
  SELFCHECK_CHECK_STREQ(std::string("{\"zebra\":1,\"alpha\":2}"), compact_order_text());
}

SELFCHECK_TEST(json_backend_formats_objects_with_tabs) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "a", cJSON_CreateNumber(12.0));
  cJSON_AddItemToObject(root, "b", cJSON_CreateString("x"));
  SELFCHECK_CHECK_STREQ(std::string("{\n\t\"a\":\t12,\n\t\"b\":\t\"x\"\n}"), print_formatted(root));
  cJSON_Delete(root);
}

SELFCHECK_TEST(json_backend_formats_nested_containers_with_one_tab_per_level) {
  cJSON *root = cJSON_CreateObject();
  cJSON *outer = cJSON_CreateObject();
  cJSON_AddItemToObject(outer, "inner", cJSON_CreateNumber(1.0));
  cJSON_AddItemToObject(root, "outer", outer);
  SELFCHECK_CHECK_STREQ(std::string("{\n\t\"outer\":\t{\n\t\t\"inner\":\t1\n\t}\n}"),
                        print_formatted(root));
  cJSON_Delete(root);
}

SELFCHECK_TEST(json_backend_formats_arrays_on_a_single_line) {
  /* cJSON 1.7.10 has no line breaking for arrays, even in formatted mode: it
     only widens the separator from "," to ", ". This differs from newer
     releases and is part of the pinned behaviour, so it is asserted here. */
  cJSON *root = cJSON_CreateArray();
  cJSON_AddItemToArray(root, cJSON_CreateNumber(1.0));
  cJSON_AddItemToArray(root, cJSON_CreateNumber(2.0));
  SELFCHECK_CHECK_STREQ(std::string("[1, 2]"), print_formatted(root));
  SELFCHECK_CHECK_STREQ(std::string("[1,2]"), print_compact(root));
  cJSON_Delete(root);
}

SELFCHECK_TEST(json_backend_parses_a_small_document) {
  const char *text = "{\"devices\":{\"sensor\":{\"index\":0,\"name\":\"sensor\"}}}";
  cJSON *root = cJSON_Parse(text);
  SELFCHECK_CHECK(root != NULL);
  if (root == NULL) {
    return;
  }

  const cJSON *devices = cJSON_GetObjectItemCaseSensitive(root, "devices");
  SELFCHECK_CHECK(cJSON_IsObject(devices));
  const cJSON *sensor =
      (devices != NULL) ? cJSON_GetObjectItemCaseSensitive(devices, "sensor") : NULL;
  SELFCHECK_CHECK(cJSON_IsObject(sensor));
  const cJSON *name = (sensor != NULL) ? cJSON_GetObjectItemCaseSensitive(sensor, "name") : NULL;
  SELFCHECK_CHECK(cJSON_IsString(name));
  if (cJSON_IsString(name)) {
    SELFCHECK_CHECK_STREQ(std::string("sensor"), std::string(name->valuestring));
  }

  cJSON_Delete(root);
}
