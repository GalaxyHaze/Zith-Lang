#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "parser/parse-result.hpp"
#include "parser/source-map.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "ast/ast-ids.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "memory/arena.hpp"

#include <cstdio>

static int failed = 0;
static int passed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); failed++; } \
    else { std::printf("  PASS: %s\n", msg); passed++; } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

using namespace zith::lexer;
using namespace zith::parser;
using namespace zith::ast;
using zith::memory::Arena;
using zith::diagnostics::DiagnosticEngine;

static Arena arena;
static DiagnosticEngine diags;
static AstBuilder builder{arena};

static void reset() {
    Arena new_arena;
    arena = std::move(new_arena);
    diags.clear();
    AstBuilder new_builder{arena};
    builder = std::move(new_builder);
}

static void test_parse_literal_expr() {
    reset();
    auto tokens = tokenize("test", "42;").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse literal program ok");
    if (!result.ok) return;

    auto prog = builder.getDecl(result.value);
    CHECK(std::holds_alternative<FnDeclNode>(prog), "program is FnDecl");
    if (!std::holds_alternative<FnDeclNode>(prog)) return;

    auto &fn = std::get<FnDeclNode>(prog);
    auto &body = builder.getExpr(fn.body);
    CHECK(std::holds_alternative<BlockNode>(body), "body is block");
    if (!std::holds_alternative<BlockNode>(body)) return;

    auto &block = std::get<BlockNode>(body);
    CHECK_EQ(block.stmts.size(), size_t(1), "block has 1 stmt");

    auto &stmt = builder.getStmt(block.stmts[0]);
    CHECK(std::holds_alternative<RetNode>(stmt), "stmt is RetNode");
}

static void test_parse_binary_expr() {
    reset();
    auto tokens = tokenize("test", "1 + 2;").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse binary expr ok");
}

static void test_parse_nested_expr() {
    reset();
    auto tokens = tokenize("test", "(1 + 2) * 3;").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse nested expr ok");
}

static void test_parse_call_expr() {
    reset();
    auto tokens = tokenize("test", "foo(1, 2);").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse call expr ok");
}

static void test_parse_if_expr() {
    reset();
    auto tokens = tokenize("test", "if true { 1 } else { 2 };").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse if expr ok");
}

int main() {
    std::printf("parser-expr tests\n");
    std::printf("===================\n\n");

    test_parse_literal_expr();
    test_parse_binary_expr();
    test_parse_nested_expr();
    test_parse_call_expr();
    test_parse_if_expr();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
