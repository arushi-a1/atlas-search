// Copyright (c) 2026 Arushi Agarwal
// SPDX-License-Identifier: MIT
//
// A tiny test framework: register TEST(name) { ... } blocks, run them all
// from main(), print a pass/fail summary, and exit non-zero on failure
// (so CI can gate on it). Not a gtest replacement for large suites, but
// keeps this repo buildable with zero extra dependencies.

#ifndef DISTRIBUTED_SEARCH_TESTS_TEST_FRAMEWORK_H_
#define DISTRIBUTED_SEARCH_TESTS_TEST_FRAMEWORK_H_

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dse_test {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& Registry() {
  static std::vector<TestCase> registry;
  return registry;
}

struct Registrar {
  Registrar(const std::string& name, std::function<void()> fn) {
    Registry().push_back({name, std::move(fn)});
  }
};

struct AssertionFailure {
  std::string message;
};

inline int RunAll() {
  int passed = 0, failed = 0;
  for (const auto& test : Registry()) {
    try {
      test.fn();
      std::cout << "[PASS] " << test.name << std::endl;
      ++passed;
    } catch (const AssertionFailure& e) {
      std::cout << "[FAIL] " << test.name << ": " << e.message << std::endl;
      ++failed;
    } catch (const std::exception& e) {
      std::cout << "[FAIL] " << test.name << " (exception): " << e.what() << std::endl;
      ++failed;
    }
  }
  std::cout << passed << " passed, " << failed << " failed" << std::endl;
  return failed == 0 ? 0 : 1;
}

}  // namespace dse_test

#define TEST(name)                                                        \
  void name();                                                            \
  static dse_test::Registrar registrar_##name(#name, name);               \
  void name()

#define ASSERT_TRUE(cond)                                                 \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::ostringstream _oss;                                           \
      _oss << "ASSERT_TRUE failed: " #cond " at " << __FILE__ << ":" << __LINE__; \
      throw dse_test::AssertionFailure{_oss.str()};                       \
    }                                                                     \
  } while (0)

#define ASSERT_EQ(a, b)                                                   \
  do {                                                                    \
    if (!((a) == (b))) {                                                  \
      std::ostringstream _oss;                                           \
      _oss << "ASSERT_EQ failed: " #a " != " #b " at " << __FILE__ << ":" << __LINE__; \
      throw dse_test::AssertionFailure{_oss.str()};                       \
    }                                                                     \
  } while (0)

#endif  // DISTRIBUTED_SEARCH_TESTS_TEST_FRAMEWORK_H_
