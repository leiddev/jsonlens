# jsonlens

[![CI](https://github.com/leiddev/jsonlens/actions/workflows/ci.yml/badge.svg)](https://github.com/leiddev/jsonlens/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++11](https://img.shields.io/badge/C%2B%2B-11-blue.svg)

A small, strict JSON library for C++11. It reads JSON without silently accepting
it, keeps object keys in the order they were written, tells an integer from a
number, and refuses to save a configuration when a required field is missing.

There is one dependency: a vendored copy of [cJSON][cjson] 1.7.10, pinned
byte-for-byte and used only as the parse and print backend. Everything a caller
touches — the value model, the path addressing, the diagnostics — is this
library.

[cjson]: https://github.com/DaveGamble/cJSON

```cpp
#include <jsonlens/jsonlens.hpp>

std::string error;
jsonlens::JsonDocument document;
if (!jsonlens::JsonDocument::load_file("report.json", document, error)) {
  std::fprintf(stderr, "%s\n", error.c_str());
  return 1;
}

long long port = 0;
if (!document.get_integer("device.port", port, error)) {
  std::fprintf(stderr, "%s\n", error.c_str());   // names the path and the kind
  return 1;
}

std::string name = document.get_string_or("device.name", "unnamed");
document.set("device.enabled", jsonlens::JsonValue::make_bool(true), error);
document.save_file("report.json", error);
```

## Why another JSON library

Most C++ JSON libraries optimise for parsing large documents quickly. This one
optimises for the smaller job of reading a configuration file that a human
wrote, and for failing loudly when that file is wrong:

- **A strict read.** The text must be exactly one JSON value. Trailing data,
  duplicate object keys, `01`, `.5`, a raw control character inside a string and
  a lone `\u` escape are errors, not things to guess about. Duplicates are
  rejected because a parser that keeps the last one turns a copy-paste mistake
  into a value nobody can find later.
- **Diagnostics a person can act on.** Every key and value is recorded with its
  line and column, and every read or write failure names the path that failed
  and the kind it found. A typo is a message, not a default.
- **Order and type are preserved.** Objects keep insertion order, so a document
  written back looks like the document that was read. `1` stays an integer and
  `1.0` stays a number, which matters when a field is an index and not a
  measurement.
- **No lifetime puzzles.** A `JsonDocument` is a value: it is copied, moved and
  destroyed like one. There is no tree to free, no handle to keep alive and no
  borrowed pointer to misuse.

## What you get

| Header | Purpose |
| --- | --- |
| `<jsonlens/value.hpp>` | `JsonValue`: an ordered JSON tree with `null`, `boolean`, `integer`, `number`, `string`, `array` and `object` kinds. |
| `<jsonlens/json_index.hpp>` | Token positions, token kinds and duplicate-key detection from a single scan of the text. Standalone: no JSON backend involved. |
| `<jsonlens/read.hpp>` | `parse_json_text()`: strict text to `JsonValue`. |
| `<jsonlens/write.hpp>` | `to_json_text()`: `JsonValue` to text, formatted or compact. |
| `<jsonlens/document.hpp>` | `JsonDocument`: typed getters and setters addressed by path. |
| `<jsonlens/version.hpp>` | Version macros and functions. |
| `<jsonlens/jsonlens.hpp>` | Umbrella header that includes all of the above. |

## Building it

The library is header-only by default. It also ships as a small static library
for projects that would rather not have one translation unit carry the backend.

### With CMake

```cmake
add_subdirectory(third_party/jsonlens)
target_link_libraries(my_target PRIVATE jsonlens::jsonlens)
```

That is the header-only form. To compile the backend once into a static
library instead:

```cmake
set(JSONLENS_COMPILED ON)      # ON by default
add_subdirectory(third_party/jsonlens)
target_link_libraries(my_target PRIVATE jsonlens::compiled)
```

`jsonlens::compiled` is a static library built from `src/implementation.cpp`
and it propagates `JSONLENS_COMPILED` to its consumers, so nothing else in the
program has to know which mode it is in.

The available options are:

| Option | Default | Meaning |
| --- | --- | --- |
| `JSONLENS_COMPILED` | `ON` | Build the `jsonlens::compiled` static library. |
| `JSONLENS_BUILD_TESTS` | `OFF` | Build the self-check test suite. |
| `JSONLENS_WARNINGS_AS_ERRORS` | `OFF` | Make the library's own warnings fatal. |
| `JSONLENS_INSTALL` | `ON` | Generate install rules and the CMake package files. |

### Without CMake

Header-only: copy `include/` and, in **exactly one** translation unit, define
`JSONLENS_IMPLEMENTATION` before including any jsonlens header that uses the
backend:

```cpp
#define JSONLENS_IMPLEMENTATION
#include <jsonlens/jsonlens.hpp>
```

Compiled: compile `src/implementation.cpp` (which defines
`JSONLENS_IMPLEMENTATION` itself) and define `JSONLENS_COMPILED` in every other
translation unit that includes the library.

Defining both macros in one translation unit is a compile error, by design.

### After installing

```cmake
find_package(jsonlens CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE jsonlens::jsonlens)   # or jsonlens::compiled
```

## Reading

### From text, with a real error message

```cpp
jsonlens::JsonValue value;
std::string error;
if (!jsonlens::parse_json_text(text, value, error)) {
  // error is something like:
  //   unexpected trailing data at line 3 column 12
  //   duplicated object key(s): device.port
  //   not valid JSON: the JSON backend rejected the document
}
```

The scanner in `json_index.hpp` decides whether the text is JSON and where every
token sits; the backend decides what each node holds. Both have to accept the
text, and the scanner is the stricter of the two.

`parse_json_text()` rejects:

| Input | Because |
| --- | --- |
| `{"a":1} {"b":2}` | The whole text must be one value. |
| `{"a":1,"a":2}` | A duplicate key is a mistake, not a precedence rule. |
| `01`, `.5`, `1.`, `+1` | JSON numbers have a fixed grammar. |
| `"tab	inside"` | Raw control characters must be escaped. |
| `[[[…]]]` past `json_nesting_limit()` | A pathological document has a bounded cost. |

The nesting limit is also what keeps the reader's recursion bounded: one read of
an over-deep document fails instead of running out of stack.

### Positions

`JsonIndex` is a standalone scanner. It needs no JSON library, so a tool that
only has to point at a line and a column can use it on its own:

```cpp
std::string error;
jsonlens::JsonIndex index;
if (!jsonlens::JsonIndex::build(text, index, error)) {
  std::fprintf(stderr, "%s\n", error.c_str());   // "... at line 4 column 9"
}

const jsonlens::JsonPosition position = index.value_position("device.port");
if (jsonlens::json_position_is_known(position)) {
  std::printf("device.port is at line %zu, column %zu\n", position.line, position.column);
}
```

## The path syntax

Every lookup and update uses the same path:

```
device.port
pads[2].name
/servers/0/host        -> the leading slash is ignored, the rest is the same syntax
```

A key that contains a dot, a bracket or a backslash is escaped with a backslash;
`escape_json_path_key()` does that for you. An empty path means the document
itself, and an empty key is not allowed, so a path is always a walk that can be
taken.

`/a/b` and `a.b` are the same path. The leading-slash form is accepted so that a
familiar JSON-pointer-looking path works without a second parser; it is not a
second syntax.

## Documents, with defaults and with failure

`JsonDocument` has two families of getters. One fails and says why; the other
takes a fallback and records why it fell back in `last_error()`:

```cpp
long long port = 0;
if (!document.get_integer("device.port", port, error)) {
  // "device.port: expected integer, found string"
  // or "device.port: there is no value at this path"
}

// A field that may be absent:
int retries = document.get_integer_or("device.retries", 3);
if (!document.last_error().empty()) {
  std::fprintf(stderr, "using the default: %s\n", document.last_error().c_str());
}
```

Available getters: `get_value`, `get_bool`, `get_integer`, `get_number`,
`get_string`, `get_array`, `get_documents` (an array of documents), and the
matching `_or` forms. `last_error_` is `mutable` so that the `_or` forms can be
`const`; a document is therefore not safe to read from two threads at once while
either of them may fall back. Reads that fail explicitly are.

`set()` creates the steps a path needs and refuses everything else:

```cpp
document.set("device.port", jsonlens::JsonValue::make_integer(115200), error);
document.set("device.tags[0]", jsonlens::JsonValue::make_string("uart"), error);
document.set("device.port[0]", jsonlens::JsonValue::make_null(), error);
// -> fails: "device.port: expected an array, found integer"
```

A missing key is inserted, an existing key keeps its position, a `null` in the
middle of a path is replaced by the container the next step needs, and anything
else is an error that names the step that could not be walked through. A write
copies the containers it walks, so a deep path in a large document costs one
copy per level — deliberate, because the alternative is a mutable view with a
lifetime problem.

## Writing

```cpp
std::string text;
std::string error;
if (!jsonlens::to_json_text(document.root(), text, error, /*formatted=*/true)) {
  return 1;
}
```

The formatted layout is TAB-indented and the compact one has no whitespace; both
are byte stable and covered by the golden tests, so a generated file does not
show up as a diff when nothing changed. Object keys come out in insertion order.

Numbers are printed by the backend, so their spelling is part of the pinned
behaviour: `1.0` prints as `1` and `-0.0` prints as `-0`, which is documented in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) along with the rest of the
backend pin.

## Files

`<jsonlens/detail/text_file.hpp>` and the file helpers on `JsonDocument`
(`load_file`, `save_file`) read and write the whole file, not a line at a time.
A text read back is the text that was written; a file that cannot be opened or
read fails with the message from the operating system rather than silently
yielding an empty string.

## Versioning

The project follows [Semantic Versioning](https://semver.org/). Until 1.0.0 the
minor version may carry API changes; the header names and the path syntax are
expected to stay stable. The version is available at compile time as
`JSONLENS_VERSION_STRING` and at run time as `jsonlens::version_string()`.

## Tests and CI

```sh
cmake -S . -B build -DJSONLENS_BUILD_TESTS=ON -DJSONLENS_COMPILED=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

The suite is self-checking and has no test framework dependency. It covers the
value model, the scanner, the backend's exact output, the document layer, the
file helpers, the version macros, and the self-containment of every public
header (each one is compiled alone).

CI runs on every push and pull request:

| | x64 | x86 |
| --- | --- | --- |
| Linux (GCC, Ubuntu 24.04) | header-only + compiled | header-only + compiled, in 32-bit mode |
| Windows (MSVC, VS 2022) | header-only + compiled, Debug + Release | header-only + compiled, Debug + Release |

The vendor job checks the SHA-256 of both cJSON files against the digests
recorded in `THIRD_PARTY_NOTICES.md`, and the format job enforces
`clang-format` 19.1.7 on the first-party sources.

## Contributing

- `clang-format` 19.1.7 is the formatter; the CI check is authoritative.
- Keep public headers self-contained: each one must compile on its own.
- A behaviour change needs a test that fails without it.
- The vendored cJSON files are not to be edited. If the pin has to move, update
  both digests and the notes together.

## License

MIT. See [LICENSE](LICENSE).

The vendored cJSON 1.7.10 is also MIT and is covered in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
