/*
 * main.cpp - Entry point of the jsonlens test binary.
 *
 * Every test is registered through the SELFCHECK_TEST macro in its own
 * translation unit. SELFCHECK_MAIN provides main() and runs the whole
 * registry, optionally filtered by the first command line argument.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 */
#include "framework/selfcheck.hpp"

SELFCHECK_MAIN()
