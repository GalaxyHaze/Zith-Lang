#include "../test-common.hpp"
#include "memory/source-map.hpp"
#include "memory/span.hpp"

#include <climits>
#include <cstdint>
#include <string_view>

using zith::memory::FileId;
using zith::memory::Loc;
using zith::memory::SourceMap;
using zith::memory::Span;

static auto source_map = SourceMap();

// ── Add file and validate ───────────────────────────────────────

static void test_add_file_valid() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "hello world");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    CHECK(sm.isValid(id), "isValid returns true for added file");
}

static void test_add_second_file() {
    SourceMap sm;
    auto r1 = sm.addFile("a.zith", "file a");
    auto r2 = sm.addFile("b.zith", "file b");

    CHECK(r1.isOk(), "first file added successfully");
    CHECK(r2.isOk(), "second file added successfully");
    if (!r1.isOk() || !r2.isOk())
        return;

    FileId id1 = r1.value();
    FileId id2 = r2.value();
    CHECK(id1 != id2, "two files get different FileIds");
    CHECK(sm.isValid(id1), "first file is valid");
    CHECK(sm.isValid(id2), "second file is valid");
}

// ── Snippet extraction ─────────────────────────────────────────

static void test_snippet_basic() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "hello world");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    auto snip = sm.snippet(Span{id, 0, 5});
    CHECK(snip.isOk(), "snippet for [0,5) succeeds");
    if (snip.isOk()) {
        CHECK_EQ(snip.value(), std::string_view("hello"), "snippet returns first 5 characters");
    }
}

static void test_snippet_at_end() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "hello");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    auto snip = sm.snippet(Span{id, 3, 3});
    CHECK(snip.isOk(), "snippet at same start/end succeeds");
    if (snip.isOk()) {
        CHECK_EQ(snip.value(), std::string_view(""),
                 "snippet at zero-length span returns empty string");
    }
}

static void test_snippet_entire_file() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "hello\nworld\n");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    auto snip = sm.snippet(Span{id, 0, 12});
    CHECK(snip.isOk(), "snippet for entire file succeeds");
    if (snip.isOk()) {
        CHECK_EQ(snip.value(), std::string_view("hello\nworld\n"), "snippet returns full content");
    }
}

// ── Invalid FileId ──────────────────────────────────────────────

static void test_is_valid_invalid_id() {
    SourceMap sm;
    FileId invalid = UINT32_MAX;
    CHECK(!sm.isValid(invalid), "isValid returns false for UINT32_MAX");
}

static void test_snippet_invalid_file() {
    SourceMap sm;
    auto snip = sm.snippet(Span{UINT32_MAX, 0, 5});
    CHECK(snip.isError(), "snippet with invalid FileId returns error");
}

static void test_loc_invalid_file() {
    SourceMap sm;
    Loc loc = sm.loc(Span{UINT32_MAX, 0, 0});
    CHECK_EQ(loc.line, size_t(0), "loc for invalid file returns line 0");
    CHECK_EQ(loc.col, size_t(0), "loc for invalid file returns col 0");
}

// ── Loc resolution ──────────────────────────────────────────────

static void test_loc_first_line() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "hello\nworld");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    Loc loc   = sm.loc(Span{id, 0, 0});
    CHECK_EQ(loc.line, size_t(1), "offset 0 is line 1");
    CHECK_EQ(loc.col, size_t(1), "offset 0 is col 1");
}

static void test_loc_second_line() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "hello\nworld");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    // "hello\n" is 6 bytes; 'w' of "world" is at offset 6
    Loc loc = sm.loc(Span{id, 6, 6});
    CHECK_EQ(loc.line, size_t(2), "offset 6 is line 2");
    CHECK_EQ(loc.col, size_t(1), "offset 6 is col 1");
}

static void test_loc_mid_line() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "hello\nworld");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    // 'o' of "world" at offset 7 (w=6, o=7, r=8, l=9, d=10)
    Loc loc = sm.loc(Span{id, 7, 7});
    CHECK_EQ(loc.line, size_t(2), "offset 7 is line 2");
    CHECK_EQ(loc.col, size_t(2), "offset 7 is col 2");
}

static void test_loc_multiple_lines() {
    SourceMap sm;
    auto result = sm.addFile("test.zith", "a\nb\nc\nd");
    CHECK(result.isOk(), "addFile succeeds");
    if (!result.isOk())
        return;

    FileId id = result.value();
    // 'a' at 0, '\n' at 1, 'b' at 2, '\n' at 3, 'c' at 4, '\n' at 5, 'd' at 6
    Loc loc_a = sm.loc(Span{id, 0, 0});
    CHECK_EQ(loc_a.line, size_t(1), "offset 0 is line 1");
    CHECK_EQ(loc_a.col, size_t(1), "offset 0 is col 1");

    Loc loc_b = sm.loc(Span{id, 2, 2});
    CHECK_EQ(loc_b.line, size_t(2), "offset 2 is line 2");
    CHECK_EQ(loc_b.col, size_t(1), "offset 2 is col 1");

    Loc loc_d = sm.loc(Span{id, 6, 6});
    CHECK_EQ(loc_d.line, size_t(4), "offset 6 is line 4");
    CHECK_EQ(loc_d.col, size_t(1), "offset 6 is col 1");
}

// ── addFile with duplicate path ─────────────────────────────────

static void test_add_file_duplicate_path() {
    SourceMap sm;
    auto r1 = sm.addFile("dup.zith", "first content");
    CHECK(r1.isOk(), "first addFile with path succeeds");
    if (!r1.isOk())
        return;

    FileId id1 = r1.value();

    // Adding same path again replaces content but returns same id (cache)
    auto r2 = sm.addFile("dup.zith", "second content");
    CHECK(r2.isOk(), "second addFile with same path succeeds");

    // It returns the same id via the cache
    CHECK_EQ(r2.value(), id1, "addFile with same path returns cached id");

    // Content should now be updated
    auto snip = sm.snippet(Span{id1, 0, 6});
    CHECK(snip.isOk(), "snippet of updated file succeeds");
    if (snip.isOk()) {
        CHECK_EQ(snip.value(), std::string_view("second"),
                 "content was updated to 'second content'");
    }
}

// ── loadFile with non-existent path ─────────────────────────────

static void test_load_file_nonexistent() {
    SourceMap sm;
    auto result = sm.loadFile("/nonexistent/path/to/file.zith");
    CHECK(result.isError(), "loadFile with non-existent path returns error");
}

// ── Multiple files with same content ────────────────────────────

static void test_add_multiple_independent_files() {
    SourceMap sm;

    auto r1 = sm.addFile("alpha.zith", "aaa");
    auto r2 = sm.addFile("beta.zith", "bbb");
    auto r3 = sm.addFile("gamma.zith", "ccc");
    CHECK(r1.isOk() && r2.isOk() && r3.isOk(), "three files added successfully");
    if (!r1.isOk() || !r2.isOk() || !r3.isOk())
        return;

    FileId id1 = r1.value();
    FileId id2 = r2.value();
    FileId id3 = r3.value();

    CHECK(id1 != id2, "alpha and beta have different ids");
    CHECK(id2 != id3, "beta and gamma have different ids");

    auto s1 = sm.snippet(Span{id1, 0, 3});
    auto s2 = sm.snippet(Span{id2, 0, 3});
    auto s3 = sm.snippet(Span{id3, 0, 3});

    CHECK(s1.isOk() && s2.isOk() && s3.isOk(), "all snippets succeed");
    if (s1.isOk())
        CHECK_EQ(s1.value(), std::string_view("aaa"), "alpha snippet correct");
    if (s2.isOk())
        CHECK_EQ(s2.value(), std::string_view("bbb"), "beta snippet correct");
    if (s3.isOk())
        CHECK_EQ(s3.value(), std::string_view("ccc"), "gamma snippet correct");
}

int main() {
    std::printf("source-map tests\n");
    std::printf("==================\n\n");

    test_add_file_valid();
    test_add_second_file();
    test_snippet_basic();
    test_snippet_at_end();
    test_snippet_entire_file();
    test_is_valid_invalid_id();
    test_snippet_invalid_file();
    test_loc_invalid_file();
    test_loc_first_line();
    test_loc_second_line();
    test_loc_mid_line();
    test_loc_multiple_lines();
    test_add_file_duplicate_path();
    test_load_file_nonexistent();
    test_add_multiple_independent_files();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
