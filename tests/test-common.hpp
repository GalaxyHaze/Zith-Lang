#pragma once
#include <cstdio>

inline int g_test_passed = 0;
inline int g_test_failed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); g_test_failed++; } \
    else { std::printf("  PASS: %s\n", msg); g_test_passed++; } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

#define TEST_MAIN(name) \
    int main() { \
        std::printf("%s tests\n", name); \
        std::printf("=====================\n\n"); \
        g_test_passed = 0; g_test_failed = 0; \
        test_##name(); \
        std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed); \
        return g_test_failed > 0 ? 1 : 0; \
    }
