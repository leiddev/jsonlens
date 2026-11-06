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

  /* An array grows at its own end: the index after the last item appends to it,
     and a larger index is an error that names the array and how many items it
     holds. Here a second frame is appended to the one built above. */
  jsonlens::JsonValue second = jsonlens::JsonValue::make_object();
  second.add("index", jsonlens::JsonValue::make_integer(1));
  second.add("lost", jsonlens::JsonValue::make_integer(2));
  second.add("rate", jsonlens::JsonValue::make_number(0.5));
  if (!document.set("frames[1]", second, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  /* A path creates the steps it needs, so an array of scalars is one write per
     item: channels does not exist yet, and the first write creates the array. */
  if (!document.set("channels[0]", jsonlens::JsonValue::make_integer(115200), error) ||
      !document.set("channels[1]", jsonlens::JsonValue::make_integer(9600), error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }

  if (!document.save_file(path, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  std::printf("wrote %s\n", path);
  std::printf("%s\n", document.to_text(true).c_str());
  return 0;
}
