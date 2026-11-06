/*
 * decimal.hpp - Decimal text of an integer, without the iostream machinery.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * Positions, counts and array subscripts all end up in the messages this
 * library reports, and a caller that formats its own diagnostics needs the same
 * spelling. Doing that through <sstream> would drag the iostream machinery into
 * every translation unit that reports something, which is exactly what a header
 * only library must avoid.
 */
#ifndef JSONLENS_DECIMAL_HPP
#define JSONLENS_DECIMAL_HPP

#include <cstddef>
#include <string>

namespace jsonlens {

/* Decimal text of an unsigned value, "0" for zero. */
inline std::string decimal_text(std::size_t value) {
  if (value == 0) {
    return "0";
  }
  std::string text;
  while (value != 0) {
    text.insert(text.begin(), static_cast<char>('0' + (value % 10)));
    value /= 10;
  }
  return text;
}

/* Decimal text of a signed value, with the minus sign when it is negative. The
   magnitude is computed in the unsigned domain so that the most negative value
   of the type, which has no positive counterpart, is still printed correctly. */
inline std::string signed_text(long long value) {
  if (value < 0) {
    return "-" + decimal_text(static_cast<std::size_t>(-(value + 1)) + 1);
  }
  return decimal_text(static_cast<std::size_t>(value));
}

} /* namespace jsonlens */

#endif /* JSONLENS_DECIMAL_HPP */
