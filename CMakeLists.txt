cmake_minimum_required(VERSION 3.16)

project(jsonlens
    VERSION 0.1.0
    DESCRIPTION "Strict JSON reader and writer with source positions"
    HOMEPAGE_URL "https://github.com/leiddev/jsonlens"
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(JSONLENS_COMPILED
    "Also build a static library that holds the JSON backend translation unit" ON)
option(JSONLENS_BUILD_TESTS
    "Build the jsonlens test suite" OFF)
option(JSONLENS_WARNINGS_AS_ERRORS
    "Treat warnings in the jsonlens targets as errors" OFF)
option(JSONLENS_INSTALL
    "Generate install and package export rules" ON)

if(JSONLENS_BUILD_TESTS)
    enable_testing()
endif()

# C++11 is the language level the headers promise; the tests and the
# implementation unit are built at exactly that level so a regression shows up
# as a compiler error rather than as a silent requirement bump.
add_library(jsonlens_compiler_flags INTERFACE)
target_compile_features(jsonlens_compiler_flags INTERFACE cxx_std_11)

function(jsonlens_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(JSONLENS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(JSONLENS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Header-only library
# ---------------------------------------------------------------------------

add_library(jsonlens INTERFACE)
add_library(jsonlens::jsonlens ALIAS jsonlens)

target_include_directories(jsonlens INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(jsonlens INTERFACE cxx_std_11)

# ---------------------------------------------------------------------------
# Optional compiled library
#
# Compiling the JSON backend once keeps it out of every translation unit that
# includes jsonlens/detail/cjson.hpp. Consumers select this build by defining
# JSONLENS_COMPILED, which the library propagates.
# ---------------------------------------------------------------------------

if(JSONLENS_COMPILED)
    add_library(jsonlens_compiled STATIC src/implementation.cpp)
    add_library(jsonlens::compiled ALIAS jsonlens_compiled)

    target_link_libraries(jsonlens_compiled PUBLIC jsonlens)

    # Interface only: the implementation unit itself defines
    # JSONLENS_IMPLEMENTATION and must not also see JSONLENS_COMPILED.
    target_compile_definitions(jsonlens_compiled INTERFACE JSONLENS_COMPILED)
    set_target_properties(jsonlens_compiled PROPERTIES
        # The alias below is what add_subdirectory users link against; an alias is
        # not exported, so the export needs the same name or find_package users
        # would have to spell the target jsonlens::jsonlens_compiled.
        EXPORT_NAME compiled
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        OUTPUT_NAME jsonlens)
    jsonlens_set_warnings(jsonlens_compiled)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(JSONLENS_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install and package export
# ---------------------------------------------------------------------------

if(JSONLENS_INSTALL)
    set(jsonlens_install_targets jsonlens)
    if(JSONLENS_COMPILED)
        list(APPEND jsonlens_install_targets jsonlens_compiled)
    endif()

    install(TARGETS ${jsonlens_install_targets}
        EXPORT jsonlensTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # The .c file travels with the headers on purpose: a consumer that defines
    # JSONLENS_IMPLEMENTATION compiles the backend from its own translation unit,
    # so cjson.hpp has to be able to include it after the install as well.
    install(DIRECTORY include/jsonlens
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.c")

    install(EXPORT jsonlensTargets
        FILE jsonlensTargets.cmake
        NAMESPACE jsonlens::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsonlens)

    configure_package_config_file(
        cmake/jsonlensConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/jsonlensConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsonlens)

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/jsonlensConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/jsonlensConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/jsonlensConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jsonlens)
endif()
