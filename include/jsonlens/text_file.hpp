/*
 * text_file.hpp - Read and write a whole text file.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * A program that reads a configuration from a file, and writes JSON back to one,
 * needs exactly two file operations: read all of a file and replace all of a
 * file. Nothing else about files belongs here - no directory handling, no paths,
 * no append, no partial reads.
 *
 * The two functions treat a file as bytes and never translate line endings, so
 * a document written on one platform is read back unchanged on another.
 *
 * write_text_file() writes a temporary file next to the target and renames it
 * over the target, so a reader either sees the whole previous document or the
 * whole new one, never a half written file. The temporary name is the target
 * name with ".jsonlens-tmp" appended, which is the only name this header ever
 * invents; it is removed again when anything fails.
 *
 * Both functions return false and fill error with a message that names no path,
 * because the caller is the one that knows which file it asked for. The path of
 * a file is a narrow string: on Windows the standard library maps it through the
 * active code page, so a path outside that code page cannot be opened. Pass a
 * path the platform can represent.
 *
 * A failure never writes the output of a function, the same rule the JSON reader
 * follows, so a caller that keeps its previous text keeps it whole and a caller
 * that checks the result cannot mistake half a file for a file.
 */
#ifndef JSONLENS_TEXT_FILE_HPP
#define JSONLENS_TEXT_FILE_HPP

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include <sys/stat.h>

namespace jsonlens {

namespace detail {

/* True when the path names a regular file, which is the only thing this header
   reads. A directory can be opened for reading on some systems and then fails
   while it is read, and libstdc++ reports that by throwing out of the stream
   rather than by setting badbit, so the check is made before the stream is
   opened: a failure is a false and a message on every platform. */
inline bool is_regular_file(const std::string &path) {
  struct stat info;
  if (stat(path.c_str(), &info) != 0) {
    return false;
  }
  /* st_mode is unsigned short on MSVC and mode_t elsewhere, so it is widened
     before it is masked: the comparison below is with S_IFREG, which is an int. */
  const unsigned int mode = static_cast<unsigned int>(info.st_mode);
  return (mode & static_cast<unsigned int>(S_IFMT)) == static_cast<unsigned int>(S_IFREG);
}

} /* namespace detail */

/* Read a whole file as bytes. Returns false and fills error on failure, and
   leaves text as it was, so text is the new document or the old one. */
inline bool read_text_file(const std::string &path, std::string &text, std::string &error) {
  if (!detail::is_regular_file(path)) {
    error = "cannot be opened for reading";
    return false;
  }
  std::ifstream stream(path.c_str(), std::ios::in | std::ios::binary);
  if (!stream) {
    error = "cannot be opened for reading";
    return false;
  }
  std::string read;
  read.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  if (stream.bad()) {
    error = "cannot be read";
    return false;
  }
  text = read;
  return true;
}

/* Replace the contents of a file with text. Returns false and fills error on
   failure, and leaves the previous contents in place when it can. */
inline bool write_text_file(const std::string &path, const std::string &text, std::string &error) {
  const std::string temporary = path + ".jsonlens-tmp";
  {
    std::ofstream stream(temporary.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream) {
      error = "cannot be opened for writing";
      return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.close();
    if (stream.fail()) {
      std::remove(temporary.c_str());
      error = "cannot be written";
      return false;
    }
  }
  if (std::rename(temporary.c_str(), path.c_str()) == 0) {
    return true;
  }
  /* A platform that refuses to rename onto an existing file gets one retry
     after the target has been removed. The window that opens for a reader is
     the price of a file system that cannot replace atomically. */
  std::remove(path.c_str());
  if (std::rename(temporary.c_str(), path.c_str()) != 0) {
    std::remove(temporary.c_str());
    error = "cannot be replaced";
    return false;
  }
  return true;
}

} /* namespace jsonlens */

#endif /* JSONLENS_TEXT_FILE_HPP */
