/*
 * compiled.cpp - A consumer that links the packaged static library.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * Nothing here names JSONLENS_COMPILED: the imported target carries the define,
 * which is what makes the two builds interchangeable for a consumer.
 */
#include <jsonlens/jsonlens.hpp>

#include <cstdio>
#include <string>

int main() {
  jsonlens::JsonDocument document;
  std::string error;
  document.set("device.port", jsonlens::JsonValue::make_integer(115200), error);
  long long port = 0;
  if (!document.get_integer("device.port", port, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  std::printf("port %lld\n", port);
  return 0;
}
