/*
 * cjson_implementation.cpp - Provides the vendored cJSON implementation.
 *
 * This translation unit is compiled into the jsonlens library when the project
 * is configured with -DJSONLENS_COMPILED=ON. In the default header only mode
 * the implementation is instead provided by one of the consumer's translation
 * units by defining JSONLENS_IMPLEMENTATION.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 */
#define JSONLENS_IMPLEMENTATION

#include <jsonlens/detail/cjson.hpp>
