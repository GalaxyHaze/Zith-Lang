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
using namespace zith::ast;
using zith::memory::Arena;
using zith::diagnostics::DiagnosticEngine;

// Parser is stubbed — parseProgram() returns {kInvalidDecl, {}, false}.
static void test_parser_returns_not_ok() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags;

    auto tokens = tokenize("test", "42;").value();
    zith::parser::Parser parser(tokens, builder, diags);
    auto result = parser.parseProgram();
    CHECK(!result.ok, "parser stub returns not ok");
}

static void test_ast_builder_literal() {
    Arena arena;
    AstBuilder builder(arena);

    auto e = builder.litExpr(LitKind::Int, "42");
    CHECK(e != kInvalidExpr, "litExpr returns valid ExprId");

    auto &node = builder.getExpr(e);
    CHECK(std::holds_alternative<LitValue>(node), "litExpr node is LitValue");
    if (!std::holds_alternative<LitValue>(node)) return;

    auto &lit = std::get<LitValue>(node);
    CHECK(lit.kind == LitKind::Int, "literal kind is Int");
    CHECK(lit.raw == "42", "literal raw is '42'");
}

static void test_ast_builder_ident() {
    Arena arena;
    AstBuilder builder(arena);

    auto e = builder.ident("foo");
    CHECK(e != kInvalidExpr, "ident returns valid ExprId");

    auto &node = builder.getExpr(e);
    CHECK(std::holds_alternative<IdentNode>(node), "ident node is IdentNode");
    if (!std::holds_alternative<IdentNode>(node)) return;

    auto &ident = std::get<IdentNode>(node);
    CHECK(ident.name == "foo", "ident name is foo");
}

static void test_ast_builder_binary() {
    Arena arena;
    AstBuilder builder(arena);

    auto lhs = builder.litExpr(LitKind::Int, "1");
    auto rhs = builder.litExpr(LitKind::Int, "2");
    auto b = builder.binary(lhs, BinaryOp::Add, rhs);
    CHECK(b != kInvalidExpr, "binary returns valid ExprId");

    auto &node = builder.getExpr(b);
    CHECK(std::holds_alternative<BinaryNode>(node), "binary node is BinaryNode");
}

static void test_ast_builder_fn_decl() {
    Arena arena;
    AstBuilder builder(arena);

    auto body = builder.block({});
    auto decl = builder.fnDecl("main", {}, body);
    CHECK(decl != kInvalidDecl, "fnDecl returns valid DeclId");

    auto &node = builder.getDecl(decl);
    CHECK(std::holds_alternative<FnDeclNode>(node), "fnDecl node is FnDeclNode");
    if (!std::holds_alternative<FnDeclNode>(node)) return;

    auto &fn = std::get<FnDeclNode>(node);
    CHECK(fn.name == "main", "fn name is main");
}

static void test_ast_builder_let_stmt() {
    Arena arena;
    AstBuilder builder(arena);

    auto init = builder.litExpr(LitKind::Int, "10");
    auto s = builder.letStmt("x", false, init);
    CHECK(s != kInvalidStmt, "letStmt returns valid StmtId");

    auto &node = builder.getStmt(s);
    CHECK(std::holds_alternative<LetNode>(node), "letStmt node is LetNode");
    if (!std::holds_alternative<LetNode>(node)) return;

    auto &let = std::get<LetNode>(node);
    CHECK(let.name == "x", "let name is x");
    CHECK(!let.mut, "let is not mutable");
}

int main() {
    std::printf("parser-expr tests\n");
    std::printf("===================\n\n");

    test_parser_returns_not_ok();
    test_ast_builder_literal();
    test_ast_builder_ident();
    test_ast_builder_binary();
    test_ast_builder_fn_decl();
    test_ast_builder_let_stmt();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
