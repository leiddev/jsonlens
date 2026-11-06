# Third Party Notices

`jsonlens` is distributed under the MIT license (see `LICENSE`). It bundles the
third party component listed below. Both licenses are MIT and are compatible.

---

## cJSON 1.7.10

- Upstream: https://github.com/DaveGamble/cJSON
- Release tag: `v1.7.10`
- License: MIT
- Vendored files:
  - `include/jsonlens/detail/cjson/cJSON.c` - 73437 bytes
  - `include/jsonlens/detail/cjson/cJSON.h` - 14882 bytes
- SHA-256 of the vendored files (must match upstream `v1.7.10` exactly):
  - `cJSON.c` = `a2470214ebeeb3ba196b0d221b77357348603f769343e1ae5be06a6630fa7169`
  - `cJSON.h` = `f1a5e0b21e357eb3abc1de9b2c494410156fe4de323aadb7221fb45b413bcbdc`

### Why this exact release is pinned

The serialized bytes a program observes are part of this library's contract, so
the pinned release is part of that contract as well. Newer cJSON releases are
**not** output recompatible, and the differences are exactly the ones a caller
can see:

- releasing numbers are printed as `%.15g` first, and re-printed as `%.17g` only
  when the shorter form does not round-trip back to the exact same `double`;
- an integer valued `double` such as `9007199254740992` is printed in full
  instead of scientific notation;
- `-0.0` is printed as `-0`;
- not-a-number and infinities are printed as `null`.

The differences against cJSON 1.7.19 were measured: out of 62 probe values only
three differed, all of them at or above `2^53`. The golden byte tests in
`tests/json_text_test.cpp` pin the formatting above, and any upgrade must be
validated against those tests before it is accepted.

### License text

```text
Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## Build and test only, not distributed

- **clang-format** is used by the continuous integration pipeline to check
  formatting. It is installed with pip and is not linked into any artifact.
