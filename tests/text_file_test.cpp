/*
 * text_file_test.cpp - Reading and writing a file, and a document through a file.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * A program reads its configuration from a file and writes its report to one, so
 * these tests pin the two file operations the library offers and the two things
 * a caller depends on them for: a text read back is the text that was written,
 * byte for byte, and a file that is written again holds the new document and not
 * the tail of the old one.
 *
 * The tests write into the build tree, in the directory the build gives them, so
 * they leave nothing behind next to the sources.
 */
#include <jsonlens/jsonlens.hpp>

#include <cstdio>
#include <string>

#include "framework/selfcheck.hpp"

#ifndef JSONLENS_TESTS_DIR
#define JSONLENS_TESTS_DIR "."
#endif

namespace {

/* A path inside the directory this test may write in. */
std::string path_in(const std::string &name) {
  return std::string(JSONLENS_TESTS_DIR) + "/" + name;
}

bool file_exists(const std::string &path) {
  std::string text;
  std::string error;
  return jsonlens::read_text_file(path, text, error);
}

} /* namespace */

SELFCHECK_TEST(a_file_read_back_is_the_same_text) {
  const std::string path = path_in("round_trip.json");
  /* Line endings and a NUL byte survive, because the file is bytes and the two
     functions never translate anything. */
  std::string text = "{";
  text += "\n\t\"a\": 1,\r\n";
  text += "\"b\": \"";
  text.push_back('\0');
  text += "\"\n}";
  std::string error;
  SELFCHECK_CHECK_MSG(jsonlens::write_text_file(path, text, error), error);
  std::string read;
  SELFCHECK_CHECK_MSG(jsonlens::read_text_file(path, read, error), error);
  SELFCHECK_CHECK_EQ(text.size(), read.size());
  for (std::size_t index = 0; index < text.size() && index < read.size(); ++index) {
    SELFCHECK_CHECK_EQ(static_cast<int>(static_cast<unsigned char>(text[index])),
                       static_cast<int>(static_cast<unsigned char>(read[index])));
  }
  std::remove(path.c_str());
}

SELFCHECK_TEST(a_second_write_replaces_the_first) {
  const std::string path = path_in("replaced.json");
  std::string error;
  SELFCHECK_CHECK_MSG(jsonlens::write_text_file(path, "one", error), error);
  SELFCHECK_CHECK_MSG(jsonlens::write_text_file(path, "a longer second text", error), error);
  std::string read;
  SELFCHECK_CHECK_MSG(jsonlens::read_text_file(path, read, error), error);
  SELFCHECK_CHECK_STREQ(std::string("a longer second text"), read);
  /* The temporary file the write goes through is not left behind. */
  SELFCHECK_CHECK(!file_exists(path + ".jsonlens-tmp"));
  std::remove(path.c_str());
}

SELFCHECK_TEST(reading_a_file_that_is_not_there_is_a_failure) {
  std::string text = "untouched";
  std::string error;
  SELFCHECK_CHECK(!jsonlens::read_text_file(path_in("no_such_file_here.json"), text, error));
  SELFCHECK_CHECK_STREQ(std::string("cannot be opened for reading"), error);
  /* A failed read says so by failing, and never by handing back half a file: the
     text keeps what it held, so a caller that checks the result cannot mistake
     it for the file. */
  SELFCHECK_CHECK_STREQ(std::string("untouched"), text);
}

SELFCHECK_TEST(writing_a_path_that_cannot_be_opened_is_a_failure) {
  const std::string path = path_in("no_such_directory/report.json");
  std::string error;
  SELFCHECK_CHECK(!jsonlens::write_text_file(path, "{}", error));
  SELFCHECK_CHECK_STREQ(std::string("cannot be opened for writing"), error);
  SELFCHECK_CHECK(!file_exists(path));
  SELFCHECK_CHECK(!file_exists(path + ".jsonlens-tmp"));
}

SELFCHECK_TEST(a_document_saves_to_a_file_and_loads_from_one) {
  const std::string path = path_in("document.json");
  jsonlens::JsonDocument document;
  std::string error;
  SELFCHECK_CHECK(document.set("device.port", jsonlens::JsonValue::make_integer(1883), error));
  SELFCHECK_CHECK(document.set("device.name", jsonlens::JsonValue::make_string("uart0"), error));
  SELFCHECK_CHECK_MSG(document.save_file(path, error, false), error);

  std::string text;
  SELFCHECK_CHECK_MSG(jsonlens::read_text_file(path, text, error), error);
  SELFCHECK_CHECK_STREQ(document.to_text(false), text);

  jsonlens::JsonDocument loaded;
  SELFCHECK_CHECK_MSG(jsonlens::JsonDocument::load_file(path, loaded, error), error);
  long long port = 0;
  SELFCHECK_CHECK_MSG(loaded.get_integer("device.port", port, error), error);
  SELFCHECK_CHECK_EQ(static_cast<long long>(1883), port);
  SELFCHECK_CHECK_STREQ(document.to_text(false), loaded.to_text(false));

  /* A file that holds a text the reader refuses fails with the message of the
     reader, and not with the message of the write that made it. */
  const std::string bad_path = path_in("bad_document.json");
  SELFCHECK_CHECK_MSG(jsonlens::write_text_file(bad_path, "{\"a\":1,\"a\":2}", error), error);
  jsonlens::JsonDocument broken;
  SELFCHECK_CHECK(!jsonlens::JsonDocument::load_file(bad_path, broken, error));
  SELFCHECK_CHECK_STREQ(std::string("duplicated object key(s): a"), error);
  SELFCHECK_CHECK(
      !jsonlens::JsonDocument::load_file(path_in("no_such_file_here.json"), broken, error));
  SELFCHECK_CHECK_STREQ(std::string("cannot be opened for reading"), error);
  std::remove(path.c_str());
  std::remove(bad_path.c_str());
}
