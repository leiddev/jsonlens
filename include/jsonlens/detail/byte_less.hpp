/*
 * byte_less.hpp - Byte wise string order that is identical on every platform.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * std::string compares its characters as plain char, which is signed on MSVC
 * and unsigned on Linux. A byte above 0x7f therefore sorts before the ASCII
 * range on Windows and after it elsewhere, so two platforms would report the
 * same duplicate keys in a different order. This library promises a stable
 * order, and the reference validator sorts with the code points of the decoded
 * text, which is what comparing unsigned bytes reproduces for ASCII keys.
 */
#ifndef JSONLENS_DETAIL_BYTE_LESS_HPP
#define JSONLENS_DETAIL_BYTE_LESS_HPP

#include <cstddef>
#include <string>

namespace jsonlens {

/* Strict weak ordering over strings, byte by byte as unsigned. */
struct ByteLess {
  bool operator()(const std::string &left, const std::string &right) const {
    const std::size_t common = left.size() < right.size() ? left.size() : right.size();
    for (std::size_t index = 0; index < common; ++index) {
      const unsigned char lhs = static_cast<unsigned char>(left[index]);
      const unsigned char rhs = static_cast<unsigned char>(right[index]);
      if (lhs != rhs) {
        return lhs < rhs;
      }
    }
    return left.size() < right.size();
  }
};

} /* namespace jsonlens */

#endif /* JSONLENS_DETAIL_BYTE_LESS_HPP */
