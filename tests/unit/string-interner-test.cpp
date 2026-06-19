#include "../test-common.hpp"
#include "memory/arena.hpp"
#include "memory/string-interner.hpp"

#include <cstdio>
#include <string>
#include <string_view>

using zith::memory::Arena;
using zith::memory::InternedId;
using zith::memory::StringInterner;

static void test_intern_basic() {
    Arena arena;
    StringInterner si(arena);

    auto id = si.intern("hello");
    CHECK(id != InternedId(-1), "first intern returns valid id");
    CHECK_EQ(id, InternedId(0), "first intern gets id 0");
}

static void test_lookup_after_intern() {
    Arena arena;
    StringInterner si(arena);

    auto id = si.intern("hello");
    auto sv = si.lookup(id);
    CHECK_EQ(sv, "hello", "lookup returns interned string");
}

static void test_intern_dedup() {
    Arena arena;
    StringInterner si(arena);

    auto id1 = si.intern("hello");
    auto id2 = si.intern("hello");
    CHECK_EQ(id1, id2, "same string returns same id");
}

static void test_intern_different() {
    Arena arena;
    StringInterner si(arena);

    auto id1 = si.intern("hello");
    auto id2 = si.intern("world");
    CHECK(id1 != id2, "different strings get different ids");
}

static void test_intern_sequential_ids() {
    Arena arena;
    StringInterner si(arena);

    auto id1 = si.intern("a");
    auto id2 = si.intern("b");
    auto id3 = si.intern("c");
    CHECK_EQ(id1, InternedId(0), "first string id 0");
    CHECK_EQ(id2, InternedId(1), "second string id 1");
    CHECK_EQ(id3, InternedId(2), "third string id 2");
}

static void test_intern_empty_string() {
    Arena arena;
    StringInterner si(arena);

    auto id = si.intern("");
    CHECK(id != InternedId(-1), "empty string can be interned");
    auto sv = si.lookup(id);
    CHECK_EQ(sv, "", "empty string lookup returns empty");
}

static void test_lookup_invalid_id() {
    Arena arena;
    StringInterner si(arena);

    si.intern("hello");
    auto sv = si.lookup(999);
    CHECK(sv.empty(), "invalid id returns empty view");

    auto sv2 = si.lookup(InternedId(-1));
    CHECK(sv2.empty(), "max id returns empty view");
}

static void test_intern_long_string() {
    Arena arena;
    StringInterner si(arena);

    std::string long_str(1000, 'x');
    auto id = si.intern(long_str);
    auto sv = si.lookup(id);
    CHECK_EQ(sv.size(), size_t(1000), "long string preserved size");
    CHECK_EQ(sv[0], 'x', "long string content preserved");
}

int main() {
    std::printf("string-interner tests\n");
    std::printf("=====================\n\n");

    test_intern_basic();
    test_lookup_after_intern();
    test_intern_dedup();
    test_intern_different();
    test_intern_sequential_ids();
    test_intern_empty_string();
    test_lookup_invalid_id();
    test_intern_long_string();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
