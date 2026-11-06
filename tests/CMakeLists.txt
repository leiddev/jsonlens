# Test suite of jsonlens.
#
# Two things are checked here:
#
#   1. The behaviour of the library, through the self-registering tests in the
#      *.cpp files next to this file.
#   2. That every public header compiles on its own. A header that only works
#      after another one has been included is a trap for a consumer, so each
#      header is compiled as a translation unit of its own.

set(jsonlens_test_sources
    main.cpp
    implementation.cpp
    json_backend_test.cpp
    json_text_test.cpp
    json_document_test.cpp
    text_file_test.cpp
    version_test.cpp)

add_executable(jsonlens_selfcheck ${jsonlens_test_sources})

target_link_libraries(jsonlens_selfcheck PRIVATE jsonlens)
jsonlens_set_warnings(jsonlens_selfcheck)

set_target_properties(jsonlens_selfcheck PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

# The file test writes next to the build rather than into the source tree.
target_compile_definitions(jsonlens_selfcheck PRIVATE
    JSONLENS_TESTS_DIR="${CMAKE_CURRENT_BINARY_DIR}")

add_test(NAME jsonlens.selfcheck COMMAND jsonlens_selfcheck)

# ---------------------------------------------------------------------------
# Header self-containment
# ---------------------------------------------------------------------------

set(jsonlens_public_headers
    jsonlens.hpp
    version.hpp
    value.hpp
    json_index.hpp
    read.hpp
    write.hpp
    document.hpp
    detail/byte_less.hpp
    detail/decimal.hpp
    detail/text_file.hpp
    detail/cjson.hpp
    detail/cjson_owner.hpp)

set(jsonlens_header_check_dir ${CMAKE_CURRENT_BINARY_DIR}/header_checks)
file(MAKE_DIRECTORY ${jsonlens_header_check_dir})

foreach(header IN LISTS jsonlens_public_headers)
    string(REPLACE "/" "_" header_slug "${header}")
    string(REPLACE "." "_" header_slug "${header_slug}")

    set(header_source ${jsonlens_header_check_dir}/check_${header_slug}.cpp)
    file(WRITE ${header_source} "#include <jsonlens/${header}>\n")

    add_library(jsonlens_header_${header_slug} OBJECT ${header_source})
    target_link_libraries(jsonlens_header_${header_slug} PRIVATE jsonlens)
    jsonlens_set_warnings(jsonlens_header_${header_slug})

    set_target_properties(jsonlens_header_${header_slug} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
endforeach()
