#include "diagnostics/diagnostic-engine.hpp"
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "lexer/keyword-table.hpp"

#include <cstdio>

static int failed = 0;
static int passed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); failed++; } \
    else { std::printf("  PASS: %s\n", msg); passed++; } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

using namespace zith::lexer;

static auto makeDiags() {
    return zith::diagnostics::DiagnosticEngine();
}

static void test_tokenize_simple_fn() {
    auto diags = makeDiags();
    auto result = tokenize("test", "fn main() {}", diags);
    CHECK(result.isOk(), "tokenize fn main");
    if (!result.isOk()) return;

    auto &ts = result.value();
    CHECK(!ts.is_empty(), "stream not empty");
    CHECK(ts.peek().is(TokenKind::Fn), "first token is Fn");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Identifier), "second token is Identifier");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Punctuation), "third token is Punctuation '('");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Punctuation), "fourth token is Punctuation ')'");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Punctuation), "fifth token is Punctuation '{'");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Punctuation), "sixth token is Punctuation '}'");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::End), "ends with End token");
}

static void test_tokenize_let() {
    auto diags = makeDiags();
    auto result = tokenize("test", "let x: i32 = 42;", diags);
    CHECK(result.isOk(), "tokenize let x");
    if (!result.isOk()) return;

    auto &ts = result.value();
    CHECK(ts.peek().is(TokenKind::Variable), "let token");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Identifier), "ident x");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Punctuation), "colon");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Type), "type i32");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Operators), "assign =");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::LitVal), "literal 42");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Punctuation), "semicolon");
}

static void test_keyword_lookup() {
    CHECK_EQ(lookup_keyword("fn"), TokenKind::Fn, "keyword 'fn' -> Fn");
    CHECK_EQ(lookup_keyword("let"), TokenKind::Variable, "keyword 'let' -> Variable");
    CHECK_EQ(lookup_keyword("struct"), TokenKind::Struct, "keyword 'struct' -> Struct");
    CHECK_EQ(lookup_keyword("if"), TokenKind::If, "keyword 'if' -> If");
    CHECK_EQ(lookup_keyword("return"), TokenKind::Control, "keyword 'return' -> Control");
    CHECK_EQ(lookup_keyword("i32"), TokenKind::Type, "keyword 'i32' -> Type");
    CHECK_EQ(lookup_keyword("bool"), TokenKind::Type, "keyword 'bool' -> Type");
    CHECK_EQ(lookup_keyword("not_a_keyword"), TokenKind::Identifier, "non-keyword -> Identifier");
    CHECK_EQ(lookup_keyword(""), TokenKind::Identifier, "empty -> Identifier");
}

static void test_token_kind_names() {
    CHECK(tokenKindName(TokenKind::Fn) != nullptr, "tokenKindName(Fn) not null");
    CHECK(tokenKindName(TokenKind::Identifier) != nullptr, "tokenKindName(Identifier) not null");
    CHECK(tokenKindName(TokenKind::End) != nullptr, "tokenKindName(End) not null");
}

static void test_token_stream_ops() {
    auto diags = makeDiags();
    auto result = tokenize("test", "a b c", diags);
    CHECK(result.isOk(), "tokenize abc");
    if (!result.isOk()) return;

    auto &ts = result.value();
    CHECK_EQ(ts.peek().kind, TokenKind::Identifier, "peek() first");
    CHECK_EQ(ts.peek(1).kind, TokenKind::Identifier, "peek(1) second");
    ts.advance();
    CHECK(ts.match(TokenKind::Identifier), "match second ident");
    CHECK_EQ(ts.peek().kind, TokenKind::Identifier, "peek() third");
    ts.advance();
    CHECK(ts.is_empty() || ts.peek().is(TokenKind::End), "stream empty or End");
}

static void test_tokenize_expression() {
    auto diags = makeDiags();
    auto result = tokenize("test", "1 + 2 * 3", diags);
    CHECK(result.isOk(), "tokenize expression");
    if (!result.isOk()) return;

    auto &ts = result.value();
    CHECK(ts.peek().is(TokenKind::LitVal), "literal 1");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Operators), "operator +");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::LitVal), "literal 2");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::Operators), "operator *");
    ts.advance();
    CHECK(ts.peek().is(TokenKind::LitVal), "literal 3");
}

static void test_tokenize_error() {
    auto diags = makeDiags();
    auto result = tokenize("test", "\"unterminated", diags);
    CHECK(result.isError(), "unterminated string returns error");
    CHECK(diags.hasErrors(), "diagnostic engine has errors");
}

int main() {
    std::printf("lexer-tokens tests\n");
    std::printf("=====================\n\n");

    test_tokenize_simple_fn();
    test_tokenize_let();
    test_keyword_lookup();
    test_token_kind_names();
    test_token_stream_ops();
    test_tokenize_expression();
    test_tokenize_error();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
