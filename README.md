# jsonlens

A small, header-only C++11 JSON library that reads and writes JSON **strictly**
and tells you **where** something went wrong - file, line and column - instead of
failing silently.

> **Status: 0.1.0 - first public release.** The API is usable but may still
> change in a 0.x minor release.

```cpp
#include <jsonlens/document.hpp>

jsonlens::JsonDocument doc;
if (!doc.parse(text)) {
    return doc.last_error();          // "line 3, column 12: ..."
}
const int port = doc.get_integer_or("device.port", 80);
```

## What it is for

Most lightweight JSON libraries parse a document into a tree and stop there.
`jsonlens` also records, for every key and every value, the position it came
from, so a later error can point at the exact line and column. It is aimed at
configuration files, where a typo must not turn into a silently wrong setting.

- Strict RFC 8259 syntax: duplicate keys, `01`, `.5`, trailing data and
  over-deep nesting are rejected.
- Positions for every node, plus path based lookup (`device.port`, `pads[2]`).
- `integer` and `number` stay distinct, as in the source document.
- Stable output bytes, pinned by golden tests.

## Building

Header-only by default: add `include/` to your include path and define
`JSONLENS_IMPLEMENTATION` in exactly one translation unit.

```cmake
add_subdirectory(jsonlens)
target_link_libraries(my_target PRIVATE jsonlens::jsonlens)
```

Or build the library that holds the backend once:

```cmake
set(JSONLENS_COMPILED ON)
add_subdirectory(jsonlens)
target_link_libraries(my_target PRIVATE jsonlens::compiled)
```

Requirements: a C++11 compiler. No dependencies beyond the standard library.

## Tests

```sh
cmake -S . -B build -DJSONLENS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## License

MIT. The bundled cJSON release is MIT as well; see `THIRD_PARTY_NOTICES.md`.
