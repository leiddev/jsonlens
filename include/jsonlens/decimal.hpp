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

namespace detail {

/* The digit loop both conversions share. Its type is at least 64 bits wide, which
   is what a long long needs, where std::size_t is 32 bits on a 32 bit platform. */
inline std::string unsigned_text(unsigned long long value) {
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

} /* namespace detail */

/* Decimal text of an unsigned value, "0" for zero. */
inline std::string decimal_text(std::size_t value) {
  return detail::unsigned_text(static_cast<unsigned long long>(value));
}

/* Decimal text of a signed value, with the minus sign when it is negative. The
   magnitude is computed in the unsigned domain so that the most negative value
   of the type, which has no positive counterpart, is still printed correctly: it
   does not fit std::size_t on a 32 bit platform, so the type here is the unsigned
   counterpart of long long and never the width of a subscript. */
inline std::string signed_text(long long value) {
  if (value < 0) {
    return "-" + detail::unsigned_text(static_cast<unsigned long long>(-(value + 1)) + 1ULL);
  }
  return detail::unsigned_text(static_cast<unsigned long long>(value));
}

} /* namespace jsonlens */

#endif /* JSONLENS_DECIMAL_HPP */
