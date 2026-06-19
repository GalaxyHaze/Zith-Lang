#include "../test-common.hpp"
#include "memory/result.hpp"

#include <cstdio>
#include <string>

using zith::memory::Error;
using zith::memory::Result;

static void test_ok_construction() {
    Result<int> r(42);
    CHECK(r.isOk(), "result with value is ok");
    CHECK(!r.isError(), "result with value is not error");
    CHECK(r, "operator bool returns true for ok");
}

static void test_error_construction() {
    Result<int> r(Error{"something failed"});
    CHECK(!r.isOk(), "result with error is not ok");
    CHECK(r.isError(), "result with error is error");
    CHECK(!r, "operator bool returns false for error");
}

static void test_value_access() {
    Result<int> r(42);
    CHECK(r.isOk(), "value access precondition");
    CHECK_EQ(r.value(), 42, "value accessor");
}

static void test_error_access() {
    Result<int> r(Error{"fail"});
    CHECK(r.isError(), "error access precondition");
    CHECK_EQ(r.error().msg, "fail", "error accessor");
}

static void test_value_mutability() {
    Result<std::string> r(std::string("hello"));
    r.value() += " world";
    CHECK_EQ(r.value(), "hello world", "value is mutable");
}

static void test_map_ok() {
    Result<int> r(21);
    auto mapped = r.map([](int x) { return x * 2; });
    CHECK(mapped.isOk(), "map of ok is ok");
    CHECK_EQ(mapped.value(), 42, "map transformation");
}

static void test_map_error() {
    Result<int> r(Error{"original"});
    auto mapped = r.map([](int x) { return x * 2; });
    CHECK(mapped.isError(), "map of error is error");
    CHECK_EQ(mapped.error().msg, "original", "map preserves error");
}

static void test_and_then_ok() {
    Result<int> r(10);
    auto chained = r.and_then([](int x) -> Result<int> { return Result<int>(x + 5); });
    CHECK(chained.isOk(), "and_then of ok is ok");
    CHECK_EQ(chained.value(), 15, "and_then chain result");
}

static void test_and_then_error() {
    Result<int> r(Error{"initial"});
    auto chained = r.and_then([](int x) -> Result<int> { return Result<int>(x + 5); });
    CHECK(chained.isError(), "and_then of error is error");
    CHECK_EQ(chained.error().msg, "initial", "and_then propagates error");
}

static void test_and_then_chained_error() {
    Result<int> r(10);
    auto chained =
        r.and_then([](int) -> Result<int> { return Result<int>(Error{"chain failed"}); });
    CHECK(chained.isError(), "and_then produces error from chain");
    CHECK_EQ(chained.error().msg, "chain failed", "and_then error message");
}

static void test_ok_with_string() {
    Result<std::string> r(std::string("test"));
    CHECK(r.isOk(), "string result ok");
    CHECK_EQ(r.value(), "test", "string result value");
}

static void test_error_with_string() {
    Result<std::string> r(Error{"not found"});
    CHECK(r.isError(), "string result error");
    CHECK_EQ(r.error().msg, "not found", "string result error message");
}

static void test_map_different_type() {
    Result<int> r(3);
    auto mapped = r.map([](int x) -> std::string { return std::to_string(x); });
    CHECK(mapped.isOk(), "map changes type");
    CHECK_EQ(mapped.value(), "3", "map changes value type");
}

int main() {
    std::printf("result tests\n");
    std::printf("=============\n\n");

    test_ok_construction();
    test_error_construction();
    test_value_access();
    test_error_access();
    test_value_mutability();
    test_map_ok();
    test_map_error();
    test_and_then_ok();
    test_and_then_error();
    test_and_then_chained_error();
    test_ok_with_string();
    test_error_with_string();
    test_map_different_type();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
