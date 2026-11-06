/*
 * header_only.cpp - A consumer that carries the backend in its own translation
 * unit.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * This is the header-only build seen from outside: nothing is linked, and the
 * installed header tree has to hold the backend source as well as the headers.
 */
#define JSONLENS_IMPLEMENTATION
#include <jsonlens/jsonlens.hpp>

#include <cstdio>
#include <string>

int main() {
  jsonlens::JsonValue value;
  std::string error;
  if (!jsonlens::parse_json_text("{\"name\":\"lens\",\"version\":1}", value, error)) {
    std::fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  const jsonlens::JsonValue *const name = value.member("name");
  std::string text;
  if (name == 0 || !name->as_string(text)) {
    return 2;
  }
  std::printf("%s %s\n", text.c_str(), jsonlens::version_string());
  std::printf("%s\n", jsonlens::to_json_text(value, false).c_str());
  return 0;
}
