/*
 * selfcheck.hpp - Minimal zero dependency assertion framework for jsonlens tests.
 *
 * Copyright (c) 2026 BTXC Embedded Development Team
 * SPDX-License-Identifier: MIT
 *
 * Why a hand written framework: the test suite must build on Windows (MSVC) and
 * on Linux (GCC) with no network access and no external dependency, and it must
 * keep compile time low. This framework is a few hundred lines and covers what
 * the suite needs: named test cases, equality and near equality checks, string
 * checks, a pass or fail summary and a non zero exit code for CTest.
 *
 * Usage:
 *   #include "framework/selfcheck.hpp"
 *   SELFCHECK_TEST(my_case) { SELFCHECK_CHECK(1 + 1 == 2); }
 *   SELFCHECK_MAIN()
 *
 * Exactly one translation unit of a test binary must use SELFCHECK_MAIN().
 * Test cases may optionally be filtered by passing a substring on the command
 * line: jsonlens_selfcheck document
 *
 * The directory is called framework and not testing on purpose: a gitignore
 * entry for the build directory "Testing" that CMake creates used to match
 * "tests/testing" on a case insensitive file system and silently keep this
 * header out of the repository.
 */
#ifndef JSONLENS_TESTS_SELFCHECK_HPP
#define JSONLENS_TESTS_SELFCHECK_HPP

#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

namespace jsonlens {
namespace selfcheck {

typedef void (*TestFunction)();

struct TestCase {
  const char *name;
  TestFunction function;
};

/* Registry of all test cases, in static initialization order. */
inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> cases;
  return cases;
}

/* Counters shared by every test case. */
inline int &check_count() {
  static int value = 0;
  return value;
}

inline int &failure_count() {
  static int value = 0;
  return value;
}

inline const char *&current_test() {
  static const char *value = "";
  return value;
}

/* Registers a test case at static initialization time. */
struct Registrar {
  Registrar(const char *name, TestFunction function) {
    TestCase entry;
    entry.name = name;
    entry.function = function;
    registry().push_back(entry);
  }
};

/* -------------------------------------------------------------------------
 * Value rendering used in failure messages.
 * ------------------------------------------------------------------------- */

inline std::string describe(bool value) {
  return value ? "true" : "false";
}

inline std::string describe(char value) {
  return std::string("'") + value + "'";
}

inline std::string describe(const char *value) {
  return value != NULL ? std::string("\"") + value + "\"" : std::string("(null)");
}

inline std::string describe(const std::string &value) {
  return std::string("\"") + value + "\"";
}

inline std::string describe(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%g", value);
  return std::string(buffer);
}

inline std::string describe(float value) {
  return describe(static_cast<double>(value));
}

template <typename T>
inline std::string describe(const T &value) {
  /* Fallback for arithmetic and opaque types: report the numeric value. */
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
  return std::string(buffer);
}

/* Text conversion for string checks, accepts const char * and std::string. */
inline std::string to_text(const char *value) {
  return value != NULL ? std::string(value) : std::string();
}

inline std::string to_text(const std::string &value) {
  return value;
}

/* -------------------------------------------------------------------------
 * Reporting.
 * ------------------------------------------------------------------------- */

inline void report_failure(const char *file, int line, const std::string &what) {
  ++failure_count();
  std::printf("  %s:%d: failure in %s\n    %s\n", file, line, current_test(), what.c_str());
  std::fflush(stdout);
}

inline void increment_check() {
  ++check_count();
}

/* -------------------------------------------------------------------------
 * Check implementations.
 * ------------------------------------------------------------------------- */

inline bool check_true(const char *file, int line, const char *expression, bool condition) {
  increment_check();
  if (condition) {
    return true;
  }
  report_failure(file, line, std::string("expected: ") + expression);
  return false;
}

template <typename Actual>
inline bool check_true_value(const char *file, int line, const char *expression,
                             const Actual &actual, bool condition) {
  increment_check();
  if (condition) {
    return true;
  }
  report_failure(file, line,
                 std::string("expected: ") + expression + "\n    actual:   " + describe(actual));
  return false;
}

/* A count is often unsigned while the literal it is compared with is signed.
   Comparing the two directly would warn under -Wsign-compare, which the CI
   build treats as an error, so both sides are widened to the same signed type
   when they are integers. Values compared this way are small counters. */
template <typename Expected, typename Actual>
inline typename std::enable_if<std::is_integral<Expected>::value && std::is_integral<Actual>::value,
                               bool>::type
values_equal(const Expected &expected, const Actual &actual) {
  return static_cast<long long>(expected) == static_cast<long long>(actual);
}

template <typename Expected, typename Actual>
inline
    typename std::enable_if<!(std::is_integral<Expected>::value && std::is_integral<Actual>::value),
                            bool>::type
    values_equal(const Expected &expected, const Actual &actual) {
  return expected == actual;
}

template <typename Expected, typename Actual>
inline bool check_equal(const char *file, int line, const char *expression,
                        const Expected &expected, const Actual &actual) {
  increment_check();
  if (values_equal(expected, actual)) {
    return true;
  }
  report_failure(file, line,
                 std::string("expected: ") + expression + "\n    expected: " + describe(expected) +
                     "\n    actual:   " + describe(actual));
  return false;
}

inline bool check_equal_text(const char *file, int line, const char *expression,
                             const std::string &expected, const std::string &actual) {
  increment_check();
  if (expected == actual) {
    return true;
  }
  report_failure(file, line,
                 std::string("expected: ") + expression + "\n    expected: " + describe(expected) +
                     "\n    actual:   " + describe(actual));
  return false;
}

inline bool check_near(const char *file, int line, const char *expression, double expected,
                       double actual, double tolerance) {
  double difference = expected - actual;
  if (difference < 0.0) {
    difference = -difference;
  }
  increment_check();
  if (difference <= tolerance) {
    return true;
  }
  report_failure(file, line,
                 std::string("expected: ") + expression + "\n    expected: " + describe(expected) +
                     "\n    actual:   " + describe(actual) +
                     "\n    tolerance:" + describe(tolerance));
  return false;
}

inline bool check_contains(const char *file, int line, const char *expression,
                           const std::string &haystack, const std::string &needle) {
  increment_check();
  if (haystack.find(needle) != std::string::npos) {
    return true;
  }
  report_failure(file, line,
                 std::string("expected: ") + expression + "\n    text:     " + describe(haystack) +
                     "\n    to contain: " + describe(needle));
  return false;
}

/* -------------------------------------------------------------------------
 * Runner.
 * ------------------------------------------------------------------------- */

inline int run_all(const char *filter) {
  const std::vector<TestCase> &cases = registry();
  int executed = 0;
  int failed = 0;

  for (std::size_t i = 0; i < cases.size(); ++i) {
    if (filter != NULL && filter[0] != '\0' &&
        std::string(cases[i].name).find(filter) == std::string::npos) {
      continue;
    }

    current_test() = cases[i].name;
    const int failures_before = failure_count();

    std::printf("[ RUN      ] %s\n", cases[i].name);
    std::fflush(stdout);
    try {
      cases[i].function();
    } catch (const std::exception &error) {
      report_failure("<unknown>", 0, std::string("unexpected exception: ") + error.what());
    } catch (...) {
      report_failure("<unknown>", 0, "unexpected unknown exception");
    }
    ++executed;

    if (failure_count() != failures_before) {
      ++failed;
      std::printf("[  FAILED  ] %s\n", cases[i].name);
    } else {
      std::printf("[       OK ] %s\n", cases[i].name);
    }
    std::fflush(stdout);
  }

  std::printf("\n%d test(s) run, %d failed, %d check(s) executed\n", executed, failed,
              check_count());
  if (executed == 0) {
    std::printf("error: no test case matched the filter\n");
    return 2;
  }
  return failed == 0 ? 0 : 1;
}

} /* namespace selfcheck */
} /* namespace jsonlens */

#define SELFCHECK_TEST(test_name)                                                                  \
  static void jsonlens_selfcheck_##test_name();                                                    \
  static const ::jsonlens::selfcheck::Registrar jsonlens_selfcheck_registrar_##test_name(          \
      #test_name, &jsonlens_selfcheck_##test_name);                                                \
  static void jsonlens_selfcheck_##test_name()

#define SELFCHECK_CHECK(expression)                                                                \
  ::jsonlens::selfcheck::check_true(__FILE__, __LINE__, #expression, (expression))

#define SELFCHECK_CHECK_MSG(expression, message)                                                   \
  ::jsonlens::selfcheck::check_true_value(__FILE__, __LINE__, #expression, (message), (expression))

#define SELFCHECK_CHECK_EQ(expected, actual)                                                       \
  ::jsonlens::selfcheck::check_equal(__FILE__, __LINE__, #expected " == " #actual, (expected),     \
                                     (actual))

#define SELFCHECK_CHECK_STREQ(expected, actual)                                                    \
  ::jsonlens::selfcheck::check_equal_text(__FILE__, __LINE__, #expected " == " #actual,            \
                                          ::jsonlens::selfcheck::to_text(expected),                \
                                          ::jsonlens::selfcheck::to_text(actual))

#define SELFCHECK_CHECK_NEAR(expected, actual, tolerance)                                          \
  ::jsonlens::selfcheck::check_near(__FILE__, __LINE__, #actual, (expected), (actual), (tolerance))

#define SELFCHECK_CHECK_CONTAINS(haystack, needle)                                                 \
  ::jsonlens::selfcheck::check_contains(__FILE__, __LINE__, #haystack, (haystack), (needle))

#define SELFCHECK_FAIL(message) ::jsonlens::selfcheck::report_failure(__FILE__, __LINE__, (message))

#define SELFCHECK_MAIN()                                                                           \
  int main(int argc, char **argv) {                                                                \
    const char *filter = (argc > 1) ? argv[1] : "";                                                \
    return ::jsonlens::selfcheck::run_all(filter);                                                 \
  }

#endif /* JSONLENS_TESTS_SELFCHECK_HPP */
