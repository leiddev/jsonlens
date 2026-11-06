/*
 * diagnose.cpp - Point at what is wrong with a file without reading it.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * Usage: jsonlens_example_diagnose <file>
 *
 * This example uses json_index.hpp and nothing else: no backend, no value model,
 * no JSONLENS_IMPLEMENTATION. The scanner answers two questions on its own -
 * where the text first breaks, and which keys repeat - so a tool that only has
 * to report a position needs no more than this header.
 *
 * Try it with examples/sample-broken.json, then with a file that has a missing
 * comma.
 */
#include <jsonlens/detail/text_file.hpp>
#include <jsonlens/json_index.hpp>

#include <cstdio>
#include <string>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <file>\n", argv[0]);
    return 2;
  }

  std::string text;
  std::string error;
  if (!jsonlens::read_text_file(argv[1], text, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  jsonlens::JsonIndex index;
  if (!jsonlens::JsonIndex::build(text, index, error)) {
    std::fprintf(stderr, "%s: %s\n", argv[1], error.c_str());
    return 1;
  }

  const std::string duplicated = index.duplicated_key_list();
  if (!duplicated.empty()) {
    std::printf("duplicated key(s): %s\n", duplicated.c_str());
  }

  const jsonlens::JsonPosition position = index.value_position("device.port");
  if (jsonlens::json_position_is_known(position)) {
    std::printf("device.port is at line %zu, column %zu\n", position.line, position.column);
  } else {
    std::printf("this file has no device.port\n");
  }
  return 0;
}
