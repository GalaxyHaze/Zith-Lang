#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "parser/parse-result.hpp"
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

static void test_parse_fn_decl() {
    reset();
    auto tokens = tokenize("test", "fn main() { 1; }").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse fn decl");
    if (!result.ok) return;

    auto prog = builder.getDecl(result.value);
    CHECK(std::holds_alternative<FnDeclNode>(prog), "program is FnDecl");
    if (!std::holds_alternative<FnDeclNode>(prog)) return;

    auto &fn = std::get<FnDeclNode>(prog);
    CHECK(fn.name == "main", "function name is main");
    CHECK_EQ(fn.params.size(), size_t(0), "no params");
}

static void test_parse_fn_with_params() {
    reset();
    auto tokens = tokenize("test", "fn add(a b) { a + b; }").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse fn with params");
    if (!result.ok) return;

    auto prog = builder.getDecl(result.value);
    auto &fn = std::get<FnDeclNode>(prog);
    CHECK(fn.name == "add", "fn name is add");
    CHECK_EQ(fn.params.size(), size_t(2), "two params");
    CHECK(fn.params[0] == "a", "first param is a");
    CHECK(fn.params[1] == "b", "second param is b");
}

static void test_parse_let_stmt() {
    reset();
    auto tokens = tokenize("test", "fn main() { let x = 1; }").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse let stmt");
}

static void test_parse_multiple_stmts() {
    reset();
    auto tokens = tokenize("test", "fn main() { let a = 1; let b = 2; a + b; }").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse multiple stmts");
}

static void test_parse_empty_fn() {
    reset();
    auto tokens = tokenize("test", "fn empty() {}").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse empty fn");
}

static void test_parse_import() {
    reset();
    auto tokens = tokenize("test", "import std.io;").value();
    Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(result.ok, "parse import");
    if (!result.ok) return;

    auto prog = builder.getDecl(result.value);
    CHECK(std::holds_alternative<ImportNode>(prog), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(prog)) return;

    auto &imp = std::get<ImportNode>(prog);
    CHECK_EQ(imp.path.size(), size_t(2), "import path has 2 parts");
}

int main() {
    std::printf("parser-stmt tests\n");
    std::printf("====================\n\n");

    test_parse_fn_decl();
    test_parse_fn_with_params();
    test_parse_let_stmt();
    test_parse_multiple_stmts();
    test_parse_empty_fn();
    test_parse_import();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
