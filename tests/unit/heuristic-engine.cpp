#include "../test-common.hpp"
#include "diagnostics/diagnostic.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-table.hpp"
#include "import/symbol-visibility.hpp"
#include "memory/arena.hpp"

// Expose private static methods for direct unit testing
#define private public
#include "sema/heuristic-engine.hpp"
#undef private

#include <string>

using namespace zith::diagnostics;
using namespace zith::import;
using namespace zith::sema;
using zith::memory::Arena;
using zith::memory::DynArray;
using zith::memory::Span;

// ── Levenshtein distance tests ──────────────────────────────────

static void test_levenshtein_empty_strings() {
    CHECK_EQ(HeuristicEngine::levenshteinDistance("", ""), size_t(0),
             "levenshteinDistance('', '') == 0");
}

static void test_levenshtein_identical() {
    CHECK_EQ(HeuristicEngine::levenshteinDistance("hello", "hello"), size_t(0),
             "levenshteinDistance('hello', 'hello') == 0");
}

static void test_levenshtein_one_char_diff() {
    CHECK_EQ(HeuristicEngine::levenshteinDistance("cat", "car"), size_t(1),
             "levenshteinDistance('cat', 'car') == 1");
}

static void test_levenshtein_completely_different() {
    CHECK_EQ(HeuristicEngine::levenshteinDistance("abc", "xyz"), size_t(3),
             "levenshteinDistance('abc', 'xyz') == 3");
}

static void test_levenshtein_transposition() {
    CHECK_EQ(HeuristicEngine::levenshteinDistance("ab", "ba"), size_t(2),
             "levenshteinDistance('ab', 'ba') == 2 (transposition = 2 substitutions)");
}

static void test_levenshtein_one_empty() {
    CHECK_EQ(HeuristicEngine::levenshteinDistance("abc", ""), size_t(3),
             "levenshteinDistance('abc', '') == 3");
    CHECK_EQ(HeuristicEngine::levenshteinDistance("", "xyz"), size_t(3),
             "levenshteinDistance('', 'xyz') == 3");
}

// ── findBestMatch tests ────────────────────────────────────────

static void test_find_best_match_single_symbol() {
    Arena arena;
    SymbolTable syms(arena);
    syms.declare("foo");

    auto best = HeuristicEngine::findBestMatch("fo", syms);
    CHECK(!best.empty(), "findBestMatch('fo') finds 'foo'");
    CHECK_EQ(best, "foo", "findBestMatch('fo') returns 'foo'");
}

static void test_find_best_match_no_close() {
    Arena arena;
    SymbolTable syms(arena);
    syms.declare("xyz");

    auto best = HeuristicEngine::findBestMatch("abc", syms);
    CHECK(best.empty(), "findBestMatch('abc') returns empty when no close match");
}

static void test_find_best_match_multiple_candidates() {
    Arena arena;
    SymbolTable syms(arena);
    syms.declare("foobar");
    syms.declare("fo");
    syms.declare("far");

    auto best = HeuristicEngine::findBestMatch("foo", syms);
    CHECK(!best.empty(), "findBestMatch('foo') finds a match");
    CHECK_EQ(best, "fo", "findBestMatch('foo') picks 'fo' (closest)");
}

static void test_find_best_match_empty_table() {
    Arena arena;
    SymbolTable syms(arena);

    auto best = HeuristicEngine::findBestMatch("anything", syms);
    CHECK(best.empty(), "findBestMatch returns empty with empty symbol table");
}

// ── generate tests ─────────────────────────────────────────────

static void test_generate_undefined_ident_suggests_match() {
    Arena arena;
    SymbolTable syms(arena);
    syms.declare("foobar");

    Diagnostic diag(arena, Severity::Error, err::UndefinedIdent, "undefined identifier 'foobaz'",
                    {});
    HeuristicEngine engine;
    DynArray<std::string> suggestions(arena);
    engine.generate(diag, syms, suggestions);

    CHECK_EQ(suggestions.size(), size_t(1), "generate produced 1 suggestion for UndefinedIdent");
    if (suggestions.size() > 0) {
        CHECK(suggestions[0].find("foobar") != std::string::npos, "suggestion mentions 'foobar'");
    }
}

static void test_generate_undefined_ident_no_close_match() {
    Arena arena;
    SymbolTable syms(arena);
    syms.declare("xyz");

    Diagnostic diag(arena, Severity::Error, err::UndefinedIdent, "undefined identifier 'aaaaa'",
                    {});
    HeuristicEngine engine;
    DynArray<std::string> suggestions(arena);
    engine.generate(diag, syms, suggestions);

    CHECK_EQ(suggestions.size(), size_t(0), "generate produces no suggestion when no close match");
}

static void test_generate_type_mismatch_returns_cast_suggestion() {
    Arena arena;
    SymbolTable syms(arena);

    Diagnostic diag(arena, Severity::Error, err::TypeMismatch, "type mismatch", {});
    HeuristicEngine engine;
    DynArray<std::string> suggestions(arena);
    engine.generate(diag, syms, suggestions);

    CHECK_EQ(suggestions.size(), size_t(1), "generate produced 1 suggestion for TypeMismatch");
    if (suggestions.size() > 0) {
        CHECK(suggestions[0].find("explicit cast") != std::string::npos,
              "TypeMismatch suggestion mentions 'explicit cast'");
    }
}

static void test_generate_unrelated_code_no_suggestions() {
    Arena arena;
    SymbolTable syms(arena);

    Diagnostic diag(arena, Severity::Error, err::UnclosedString, "unclosed string literal", {});
    HeuristicEngine engine;
    DynArray<std::string> suggestions(arena);
    engine.generate(diag, syms, suggestions);

    CHECK_EQ(suggestions.size(), size_t(0),
             "generate produces no suggestions for unrelated error code");
}

static void test_generate_undefined_ident_no_quotes_in_message() {
    Arena arena;
    SymbolTable syms(arena);
    syms.declare("foo");

    Diagnostic diag(arena, Severity::Error, err::UndefinedIdent,
                    "undefined identifier (no quotes here)", {});
    HeuristicEngine engine;
    DynArray<std::string> suggestions(arena);
    engine.generate(diag, syms, suggestions);

    CHECK_EQ(suggestions.size(), size_t(0), "no suggestion when message has no quoted identifier");
}

int main() {
    std::printf("heuristic-engine tests\n");
    std::printf("======================\n\n");

    // Levenshtein distance
    test_levenshtein_empty_strings();
    test_levenshtein_identical();
    test_levenshtein_one_char_diff();
    test_levenshtein_completely_different();
    test_levenshtein_transposition();
    test_levenshtein_one_empty();

    // findBestMatch
    test_find_best_match_single_symbol();
    test_find_best_match_no_close();
    test_find_best_match_multiple_candidates();
    test_find_best_match_empty_table();

    // generate
    test_generate_undefined_ident_suggests_match();
    test_generate_undefined_ident_no_close_match();
    test_generate_type_mismatch_returns_cast_suggestion();
    test_generate_unrelated_code_no_suggestions();
    test_generate_undefined_ident_no_quotes_in_message();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
