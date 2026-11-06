/*
 * write_report.cpp - Build a document in code and write it out, in the order the
 * fields were added and with a layout that does not change between runs.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * Usage: jsonlens_example_write_report [file]
 *
 * Run it twice and diff the two files: a document that did not change does not
 * produce a diff. That is what makes a generated file worth committing.
 */
#define JSONLENS_IMPLEMENTATION
#include <jsonlens/jsonlens.hpp>

#include <cstdio>
#include <string>

int main(int argc, char **argv) {
  const char *const path = (argc > 1) ? argv[1] : "report.json";

  jsonlens::JsonValue frame = jsonlens::JsonValue::make_object();
  frame.add("index", jsonlens::JsonValue::make_integer(0));
  frame.add("lost", jsonlens::JsonValue::make_integer(0));
  frame.add("rate", jsonlens::JsonValue::make_number(1.0));

  jsonlens::JsonValue frames = jsonlens::JsonValue::make_array();
  frames.add(frame);

  jsonlens::JsonValue report = jsonlens::JsonValue::make_object();
  report.add("pipeline", jsonlens::JsonValue::make_string("capture"));
  report.add("frames", frames);

  jsonlens::JsonDocument document = jsonlens::JsonDocument::of(report);
  std::string error;
  if (!document.save_file(path, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  std::printf("wrote %s\n", path);
  std::printf("%s\n", document.to_text(true).c_str());
  return 0;
}
