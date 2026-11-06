/*
 * cjson.hpp - Access to the vendored cJSON 1.7.10 backend.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * cJSON is a C library, therefore exactly one translation unit has to provide
 * its implementation. Two modes are supported:
 *
 *   1. Header only (default). Define JSONLENS_IMPLEMENTATION in exactly
 *      one translation unit before including any jsonlens JSON header. All
 *      other translation units include the header normally and only see the
 *      declarations.
 *
 *   2. Compiled. Configure the project with -DJSONLENS_COMPILED=ON. The
 *      implementation then comes from the jsonlens library and consumers must
 *      not define JSONLENS_IMPLEMENTATION.
 *
 * The pinned release is part of the serialization compatibility contract; see
 * THIRD_PARTY_NOTICES.md before changing it.
 */
#ifndef JSONLENS_DETAIL_CJSON_HPP
#define JSONLENS_DETAIL_CJSON_HPP

#if defined(JSONLENS_IMPLEMENTATION) && defined(JSONLENS_COMPILED)
#error                                                                                             \
    "jsonlens: do not define JSONLENS_IMPLEMENTATION when linking the compiled library (JSONLENS_COMPILED is set)."
#endif

#include "cjson/cJSON.h"

/*
 * The pinned release as a string, derived from the vendored header so that it
 * can never drift away from the actual code.
 */
#define JSONLENS_DETAIL_STRINGIZE_IMPL(text) #text
#define JSONLENS_DETAIL_STRINGIZE(text) JSONLENS_DETAIL_STRINGIZE_IMPL(text)
#define JSONLENS_CJSON_VERSION                                                                     \
  JSONLENS_DETAIL_STRINGIZE(CJSON_VERSION_MAJOR)                                                   \
  "." JSONLENS_DETAIL_STRINGIZE(CJSON_VERSION_MINOR) "." JSONLENS_DETAIL_STRINGIZE(                \
      CJSON_VERSION_PATCH)

#if defined(JSONLENS_IMPLEMENTATION)
/*
 * Vendored third party code is compiled as C++ here. Its diagnostics are not
 * part of the jsonlens warning policy: MSVC reports the C4996 deprecation
 * warnings for sprintf, sscanf and strcpy, while GCC and Clang are already
 * clean with -Wall -Wextra -Wpedantic -Werror.
 */
#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#endif

#include "cjson/cJSON.c"

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/*
 * cJSON.c defines true and false as object like macros on top of its own
 * cJSON_bool type. MSVC then rejects the C++ standard headers, which check for
 * macro redefinition of these keywords via <xkeycheck.h>. Restore the language
 * level meaning right after the implementation has been compiled.
 */
#if defined(true)
#undef true
#endif
#if defined(false)
#undef false
#endif
#endif /* JSONLENS_IMPLEMENTATION */

#endif /* JSONLENS_DETAIL_CJSON_HPP */
