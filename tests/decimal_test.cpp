/*
 * decimal_test.cpp - Decimal text of an integer, without iostreams.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * Positions, counts and array subscripts end up in the messages of this library,
 * and a caller that formats its own diagnostics uses the same two functions. The
 * one value that a hand written conversion gets wrong is the most negative one of
 * the signed type, which has no positive counterpart: computing its magnitude as
 * `-value` is undefined behaviour and prints the wrong text on a two's complement
 * machine, so that case is pinned here.
 */
#include <jsonlens/decimal.hpp>
#include <jsonlens/json_index.hpp>

#include <cstddef>
#include <string>

#include "framework/selfcheck.hpp"

SELFCHECK_TEST(decimal_text_writes_an_unsigned_value) {
  SELFCHECK_CHECK_STREQ(std::string("0"), jsonlens::decimal_text(0));
  SELFCHECK_CHECK_STREQ(std::string("1"), jsonlens::decimal_text(1));
  SELFCHECK_CHECK_STREQ(std::string("9"), jsonlens::decimal_text(9));
  SELFCHECK_CHECK_STREQ(std::string("10"), jsonlens::decimal_text(10));
  SELFCHECK_CHECK_STREQ(std::string("100"), jsonlens::decimal_text(100));
  SELFCHECK_CHECK_STREQ(std::string("1234567890"), jsonlens::decimal_text(1234567890));
  /* A value of every decimal length, so that the digit loop is not only ever
     entered with two or three characters. */
  SELFCHECK_CHECK_STREQ(std::string("999"), jsonlens::decimal_text(999));
  SELFCHECK_CHECK_STREQ(std::string("1000"), jsonlens::decimal_text(1000));
  /* The largest value of std::size_t, which is not the same number on a 32 bit
     platform as on a 64 bit one: the loop is written for whatever width the type
     has, and this is what says so. */
  const std::size_t largest = ~static_cast<std::size_t>(0);
  SELFCHECK_CHECK_STREQ(sizeof(std::size_t) == 8 ? std::string("18446744073709551615")
                                                 : std::string("4294967295"),
                        jsonlens::decimal_text(largest));
}

SELFCHECK_TEST(signed_text_writes_a_minus_sign_and_the_magnitude) {
  SELFCHECK_CHECK_STREQ(std::string("0"), jsonlens::signed_text(0));
  SELFCHECK_CHECK_STREQ(std::string("42"), jsonlens::signed_text(42));
  SELFCHECK_CHECK_STREQ(std::string("-1"), jsonlens::signed_text(-1));
  SELFCHECK_CHECK_STREQ(std::string("-42"), jsonlens::signed_text(-42));
  SELFCHECK_CHECK_STREQ(std::string("-100"), jsonlens::signed_text(-100));
}

/* The most negative value of the type has no positive counterpart, so its
   magnitude cannot be computed as -value. The implementation reaches it through
   -(value + 1) + 1, and this test is what protects that detour. */
SELFCHECK_TEST(signed_text_writes_the_most_negative_value) {
  const long long lowest = -9223372036854775807LL - 1;
  SELFCHECK_CHECK_STREQ(std::string("-9223372036854775808"), jsonlens::signed_text(lowest));
  SELFCHECK_CHECK_STREQ(std::string("9223372036854775807"),
                        jsonlens::signed_text(9223372036854775807LL));
  /* One above the lowest is the value whose magnitude fits the signed type
     again, which is the boundary the detour exists for. */
  SELFCHECK_CHECK_STREQ(std::string("-9223372036854775807"), jsonlens::signed_text(lowest + 1));
}

/* The names are what a message carries, so every kind has one and none of them is
   the fallback of an unhandled enum. */
SELFCHECK_TEST(token_kind_names_cover_every_kind) {
  SELFCHECK_CHECK_STREQ(std::string("missing"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::missing));
  SELFCHECK_CHECK_STREQ(std::string("object"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::object));
  SELFCHECK_CHECK_STREQ(std::string("array"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::array));
  SELFCHECK_CHECK_STREQ(std::string("string"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::string));
  SELFCHECK_CHECK_STREQ(std::string("integer"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::integer));
  SELFCHECK_CHECK_STREQ(std::string("number"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::number));
  SELFCHECK_CHECK_STREQ(std::string("boolean"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::boolean));
  SELFCHECK_CHECK_STREQ(std::string("null"),
                        jsonlens::json_token_kind_name(jsonlens::JsonTokenKind::null_value));
}
