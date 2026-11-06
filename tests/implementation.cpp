/*
 * implementation.cpp - Provides the cJSON implementation for the tests.
 *
 * In the default header only mode exactly one translation unit has to define
 * JSONLENS_IMPLEMENTATION. When the project is configured with
 * -DJSONLENS_COMPILED=ON the implementation already comes from the jsonlens
 * library and this file only includes the declarations.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 */
#ifndef JSONLENS_COMPILED
#define JSONLENS_IMPLEMENTATION
#endif

#include <jsonlens/detail/cjson.hpp>
