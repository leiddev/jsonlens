# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Rewrote the header documentation and the README for a library that stands on
  its own: no references to the internal project the code was extracted from.

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
