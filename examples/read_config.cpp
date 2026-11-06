/*
 * read_config.cpp - Read a configuration the way a program should: ask for what
 * it needs, take a default where a default is honest, and say where the text
 * went wrong instead of guessing.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * Usage: jsonlens_example_read_config <file>
 *
 * Try it with examples/sample-config.json. Then edit one field into a string
 * and run it again: the message names the path and the kind it found.
 */
#define JSONLENS_IMPLEMENTATION
#include <jsonlens/jsonlens.hpp>

#include <cstdio>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <file>\n", argv[0]);
    return 2;
  }

  jsonlens::JsonDocument document;
  std::string error;
  if (!jsonlens::JsonDocument::load_file(argv[1], document, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  /* A field the program cannot work without: no default, no fallback. */
  long long port = 0;
  if (!document.get_integer("device.port", port, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  /* A field with a default. The value is the fallback when the field is missing
     or of the wrong kind, and last_error() still says which of the two it was. */
  const long long retries = document.get_integer_or("device.retries", 3);
  if (!document.last_error().empty()) {
    std::fprintf(stderr, "using the default: %s\n", document.last_error().c_str());
  }

  const std::string name = document.get_string_or("device.name", "unnamed");

  /* A scalar array. get_int_array() checks every item, so one item of another
     kind fails the read and is named, instead of being skipped or truncated. */
  std::vector<long long> channels;
  if (!document.get_int_array("device.channels", channels, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  /* A table of records: the array reads as documents, and each one reads its own
     fields by the same path syntax. */
  std::vector<jsonlens::JsonDocument> pads;
  if (!document.get_documents("pads", pads, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  std::printf("device %s, port %lld, retries %lld\n", name.c_str(), port, retries);
  std::printf("  channels:");
  for (std::size_t index = 0; index < channels.size(); ++index) {
    std::printf(" %lld", channels[index]);
  }
  std::printf("\n");
  for (std::size_t index = 0; index < pads.size(); ++index) {
    std::printf("  pad %s: %s\n", pads[index].get_string_or("name", "?").c_str(),
                pads[index].get_bool_or("enabled", false) ? "enabled" : "disabled");
  }
  return 0;
}
