#
# CMakeLists.txt - The examples.
#
# Copyright (c) 2026 BTXC Embedded Development Team
# SPDX-License-Identifier: MIT
#
# Each example is header-only: it defines JSONLENS_IMPLEMENTATION in its own
# source, which is the shortest way to use the library and the one a reader can
# copy into a scratch project. The scanner example does not define it at all,
# because json_index.hpp needs no backend.
#
# The examples are built only with JSONLENS_BUILD_EXAMPLES=ON and they are not
# installed: they are documentation that the compiler checks.
add_executable(jsonlens_example_read_config read_config.cpp)
target_link_libraries(jsonlens_example_read_config PRIVATE jsonlens::jsonlens)
jsonlens_set_warnings(jsonlens_example_read_config)

add_executable(jsonlens_example_write_report write_report.cpp)
target_link_libraries(jsonlens_example_write_report PRIVATE jsonlens::jsonlens)
jsonlens_set_warnings(jsonlens_example_write_report)

add_executable(jsonlens_example_diagnose diagnose.cpp)
target_link_libraries(jsonlens_example_diagnose PRIVATE jsonlens::jsonlens)
jsonlens_set_warnings(jsonlens_example_diagnose)
