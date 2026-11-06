#
# CMakeLists.txt - A consumer of the installed jsonlens package.
#
# Copyright (c) 2026 BTXC Embedded Development Team
# SPDX-License-Identifier: MIT
#
# This project is not part of the library build. It is what the package job in
# .github/workflows/ci.yml builds after cmake --install, so that the install
# rules, the exported target names and the package files are exercised by
# something that only sees the installed tree.
#
# Both build modes are used, because they find the library in different places:
# the header-only program compiles cJSON from the installed header tree, and the
# compiled program links the installed static library.
cmake_minimum_required(VERSION 3.20)
project(jsonlens_consumer LANGUAGES CXX)

find_package(jsonlens 0.1 CONFIG REQUIRED)

add_executable(header_only src/header_only.cpp)
target_link_libraries(header_only PRIVATE jsonlens::jsonlens)

if(TARGET jsonlens::compiled)
  add_executable(compiled src/compiled.cpp)
  target_link_libraries(compiled PRIVATE jsonlens::compiled)
endif()
