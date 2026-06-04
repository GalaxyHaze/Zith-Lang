// tests/test.h — Minimal test framework (~50 lines)
// No external dependencies, C++17
#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

struct TestCase {
    const char *name;
    const char *tags;
    std::function<void()> fn;
};

inline std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> v;
    return v;
}

#define CONCAT_(a, b) a ## b
#define CONCAT(a, b) CONCAT_(a, b)

#define TEST_CASE(name, tags) \
    static void CONCAT(zith_test_fn_, __LINE__)(); \
    namespace { struct CONCAT(ZithTestReg_, __LINE__) { CONCAT(ZithTestReg_, __LINE__)() { \
        test_registry().push_back({name, tags, CONCAT(zith_test_fn_, __LINE__)}); \
    }} CONCAT(zith_test_reg_, __LINE__); } \
    static void CONCAT(zith_test_fn_, __LINE__)()

#define REQUIRE(expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        std::exit(1); \
    } \
} while(0)

#define REQUIRE_FALSE(expr) REQUIRE(!(expr))

inline int run_all_tests() {
    int ran = 0, failed = 0;
    for (auto &tc : test_registry()) {
        std::printf("  %s [%s]... ", tc.name, tc.tags);
        std::fflush(stdout);
        try {
            tc.fn();
            std::printf("OK\n");
        } catch (...) {
            std::printf("FAIL (exception)\n");
            failed++;
        }
        ran++;
    }
    std::printf("\n%d tests, %d failed\n", ran, failed);
    return failed > 0 ? 1 : 0;
}

#define TEST_MAIN() int main() { return run_all_tests(); }
