# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `examples/`: three programs that read a configuration, diagnose a file with
  the scanner alone, and build a report in code, built with
  `JSONLENS_BUILD_EXAMPLES=ON` and run by the package jobs of CI.

### Changed

- Rewrote the header documentation and the README for a library that stands on
  its own: no references to the internal project the code was extracted from.

### Fixed

- A failed read no longer leaks the backend tree when the conversion into the
  model throws; the tree is owned by a class with a destructor.
- An integer literal at or above 2^53 is reported as a number instead of being
  cast to a `long long` that is not the number that was written, which was also
  undefined behaviour for a literal too large for the type.
- A number whose exponent overflows a double is an error that names the path,
  not an infinity that is written back as `null`.
- A NUL byte in the text is rejected with its offset before the backend can read
  a shorter text than the scanner checked, and a `\u0000` escape in a string is
  rejected for the same reason.
- The text must be valid UTF-8, and a leading byte order mark, an overlong form,
  a surrogate and a code point past U+10FFFF are errors. A `\u` escape that is
  half of a surrogate pair is an error instead of three bytes that are not UTF-8.
- A key that holds a NUL byte is refused when writing, as a string already was.
- The install rules did not ship `cJSON.c`, so a header-only consumer of an
  installed package could not compile the backend it includes.
- The static library was exported as `jsonlens::jsonlens_compiled`, which meant
  `find_package` users could not link the `jsonlens::compiled` that an
  `add_subdirectory` user links. The export name is now the same.

## [0.1.0] - 2026-11-06

### Added

- First public release, extracted from the JSON subsystem of an internal C++
  project.
- Strict RFC 8259 scanner that records the line and column of every key and
  value, detects duplicate keys and bounds the nesting depth.
- `jsonlens::JsonValue`: ordered value model that keeps `integer` and `number`
  apart.
- `jsonlens::parse_json_text` and `jsonlens::to_json_text`: strict reading and
  byte stable writing.
- `jsonlens::JsonDocument`: path addressed access with typed getters, defaults,
  writes and file load/save.
- Header-only and compiled build modes, CMake package export, CI on Windows and
  Linux for x86 and x64.
