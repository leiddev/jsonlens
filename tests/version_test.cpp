/*
 * version_test.cpp - The version accessors agree with each other.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 */
#include <string>

#include <jsonlens/detail/decimal.hpp>
#include <jsonlens/version.hpp>

#include "framework/selfcheck.hpp"

SELFCHECK_TEST(version_components_agree_with_the_version_string) {
  const std::string expected =
      jsonlens::detail::decimal_text(static_cast<std::size_t>(jsonlens::version_major())) + "." +
      jsonlens::detail::decimal_text(static_cast<std::size_t>(jsonlens::version_minor())) + "." +
      jsonlens::detail::decimal_text(static_cast<std::size_t>(jsonlens::version_patch()));

  SELFCHECK_CHECK_STREQ(expected, std::string(jsonlens::version_string()));
  SELFCHECK_CHECK(jsonlens::version_major() >= 0);
  SELFCHECK_CHECK(jsonlens::version_minor() >= 0);
  SELFCHECK_CHECK(jsonlens::version_patch() >= 0);
}
