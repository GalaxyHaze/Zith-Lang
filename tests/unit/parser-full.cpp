#include "../test-common.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-ids.hpp"
#include "ast/ast-nodes.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "import/symbol-table.hpp"
#include "lexer/lexer.hpp"
#include "memory/arena.hpp"
#include "memory/source-map.hpp"
#include "parser/parser.hpp"
#include "parser/scan-result.hpp"

auto parser_full_source_map = zith::memory::SourceMap();

using namespace zith::lexer;
using namespace zith::ast;
using zith::diagnostics::DiagnosticEngine;
using zith::import::SymbolTable;
using zith::memory::Arena;
using zith::memory::DynArray;
using zith::parser::Parser;
using zith::parser::scan;
using zith::parser::ScanResult;

// ── helpers ──────────────────────────────────────────────────────────

static ExprId parseExprStr(Arena &arena, AstBuilder &bld, DiagnosticEngine &diags,
                           std::string_view src) {
    auto toks = tokenize(parser_full_source_map, arena, "test", std::string(src), diags).value();
    Parser p(&toks, &bld, &diags);
    return p.parseExpr();
}

// ── binary operator tests ────────────────────────────────────────────

static void test_parse_binary_add() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 + 2");
    CHECK(e != kInvalidExpr, "parsed binary + expression");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "binary + node is BinaryNode");
    if (!std::holds_alternative<BinaryNode>(bld.getExpr(e)))
        return;
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Add, "binary + op is Add");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(bin.lhs)), "+ lhs is literal");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(bin.rhs)), "+ rhs is literal");
}

static void test_parse_binary_sub() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "10 - 3");
    CHECK(e != kInvalidExpr, "parsed binary - expression");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "binary - node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Sub, "binary - op is Sub");
}

static void test_parse_binary_mul() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "4 * 5");
    CHECK(e != kInvalidExpr, "parsed binary * expression");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "binary * node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Mul, "binary * op is Mul");
}

static void test_parse_binary_div() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "20 / 4");
    CHECK(e != kInvalidExpr, "parsed binary / expression");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "binary / node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Div, "binary / op is Div");
}

static void test_parse_binary_rest() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "10 % 3");
    CHECK(e != kInvalidExpr, "parsed binary % expression");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "binary % node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Rest, "binary % op is Rest");
}

static void test_parse_binary_mul_precedence() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    // 1 + 2 * 3 should parse as 1 + (2 * 3)
    auto e = parseExprStr(arena, bld, diags, "1 + 2 * 3");
    CHECK(e != kInvalidExpr, "parsed 1 + 2 * 3");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "root is BinaryNode");
    auto &root = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(root.op == BinaryOp::Add, "root op is Add");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(root.lhs)), "Add lhs is literal");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(root.rhs)), "Add rhs is BinaryNode (*)");
    auto &mul = std::get<BinaryNode>(bld.getExpr(root.rhs));
    CHECK(mul.op == BinaryOp::Mul, "rhs op is Mul");
}

static void test_parse_binary_add_precedence() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    // 2 * 3 + 1 should parse as (2 * 3) + 1
    auto e = parseExprStr(arena, bld, diags, "2 * 3 + 1");
    CHECK(e != kInvalidExpr, "parsed 2 * 3 + 1");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "root is BinaryNode");
    auto &root = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(root.op == BinaryOp::Add, "root op is Add");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(root.lhs)), "Add lhs is BinaryNode (*)");
    auto &mul = std::get<BinaryNode>(bld.getExpr(root.lhs));
    CHECK(mul.op == BinaryOp::Mul, "lhs op is Mul");
}

static void test_parse_binary_chained() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    // 1 + 2 + 3 should be left-associative: (1 + 2) + 3
    auto e = parseExprStr(arena, bld, diags, "1 + 2 + 3");
    CHECK(e != kInvalidExpr, "parsed 1 + 2 + 3");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "root is BinaryNode");
    auto &root = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(root.op == BinaryOp::Add, "root op is Add");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(root.lhs)), "root lhs is BinaryNode");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(root.rhs)), "root rhs is literal 3");
}

// ── compound operator tests (==, !=, <=, >=, &&, ||, <<, >>) ────────

static void test_parse_compound_eq() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 == 2");
    CHECK(e != kInvalidExpr, "parsed 1 == 2");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "== node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Eq, "== op is Eq");
}

static void test_parse_compound_ne() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 != 2");
    CHECK(e != kInvalidExpr, "parsed 1 != 2");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "!= node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Ne, "!= op is Ne");
}

static void test_parse_compound_le() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 <= 2");
    CHECK(e != kInvalidExpr, "parsed 1 <= 2");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "<= node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Le, "<= op is Le");
}

static void test_parse_compound_ge() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "3 >= 2");
    CHECK(e != kInvalidExpr, "parsed 3 >= 2");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), ">= node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Ge, ">= op is Ge");
}

static void test_parse_compound_shl() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 << 2");
    CHECK(e != kInvalidExpr, "parsed 1 << 2");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "<< node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Shl, "<< op is Shl");
}

static void test_parse_compound_shr() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "8 >> 2");
    CHECK(e != kInvalidExpr, "parsed 8 >> 2");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), ">> node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Shr, ">> op is Shr");
}

static void test_parse_compound_and() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 && 0");
    CHECK(e != kInvalidExpr, "parsed 1 && 0");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "&& node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::And, "&& op is And");
}

static void test_parse_compound_or() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 || 0");
    CHECK(e != kInvalidExpr, "parsed 1 || 0");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "|| node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Or, "|| op is Or");
}

// ── word operator tests (and, or, xor) ───────────────────────────────

static void test_parse_word_and() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 and 0");
    CHECK(e != kInvalidExpr, "parsed 1 and 0");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "and node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::And, "and op is And");
}

static void test_parse_word_or() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 or 0");
    CHECK(e != kInvalidExpr, "parsed 1 or 0");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "or node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Or, "or op is Or");
}

static void test_parse_word_xor() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "1 xor 0");
    CHECK(e != kInvalidExpr, "parsed 1 xor 0");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "xor node is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Xor, "xor op is Xor");
}

// ── unary prefix operator tests ──────────────────────────────────────

static void test_parse_unary_neg() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "-5");
    CHECK(e != kInvalidExpr, "parsed -5");
    CHECK(std::holds_alternative<UnaryNode>(bld.getExpr(e)), "-5 node is UnaryNode");
    auto &un = std::get<UnaryNode>(bld.getExpr(e));
    CHECK(un.op == UnaryOp::Neg, "- op is Neg");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(un.operand)), "neg operand is literal");
}

static void test_parse_unary_not() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "!true");
    CHECK(e != kInvalidExpr, "parsed !true");
    CHECK(std::holds_alternative<UnaryNode>(bld.getExpr(e)), "!true node is UnaryNode");
    auto &un = std::get<UnaryNode>(bld.getExpr(e));
    CHECK(un.op == UnaryOp::Not, "! op is Not");
}

static void test_parse_unary_ref() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "&x");
    CHECK(e != kInvalidExpr, "parsed &x");
    CHECK(std::holds_alternative<UnaryNode>(bld.getExpr(e)), "&x node is UnaryNode");
    auto &un = std::get<UnaryNode>(bld.getExpr(e));
    CHECK(un.op == UnaryOp::Ref, "& op is Ref");
}

static void test_parse_unary_deref() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "*p");
    CHECK(e != kInvalidExpr, "parsed *p");
    CHECK(std::holds_alternative<UnaryNode>(bld.getExpr(e)), "*p node is UnaryNode");
    auto &un = std::get<UnaryNode>(bld.getExpr(e));
    CHECK(un.op == UnaryOp::Deref, "* op is Deref");
}

static void test_parse_unary_word_not() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "not true");
    CHECK(e != kInvalidExpr, "parsed not true");
    CHECK(std::holds_alternative<UnaryNode>(bld.getExpr(e)), "not node is UnaryNode");
    auto &un = std::get<UnaryNode>(bld.getExpr(e));
    CHECK(un.op == UnaryOp::Not, "not op is Not");
}

// ── call, field, index tests ─────────────────────────────────────────

static void test_parse_call_no_args() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "foo()");
    CHECK(e != kInvalidExpr, "parsed foo()");
    CHECK(std::holds_alternative<CallNode>(bld.getExpr(e)), "foo() is CallNode");
    auto &call = std::get<CallNode>(bld.getExpr(e));
    CHECK(std::holds_alternative<IdentNode>(bld.getExpr(call.callee)), "callee is ident");
    CHECK(call.args.size() == 0, "call has 0 args");
}

static void test_parse_call_one_arg() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "foo(42)");
    CHECK(e != kInvalidExpr, "parsed foo(42)");
    CHECK(std::holds_alternative<CallNode>(bld.getExpr(e)), "foo(42) is CallNode");
    auto &call = std::get<CallNode>(bld.getExpr(e));
    CHECK(call.args.size() == 1, "call has 1 arg");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(call.args[0])), "arg is literal");
}

static void test_parse_call_two_args() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "add(1, 2)");
    CHECK(e != kInvalidExpr, "parsed add(1, 2)");
    auto &call = std::get<CallNode>(bld.getExpr(e));
    CHECK(call.args.size() == 2, "call has 2 args");
}

static void test_parse_call_chained() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "foo()()");
    CHECK(e != kInvalidExpr, "parsed foo()()");
    CHECK(std::holds_alternative<CallNode>(bld.getExpr(e)), "outer is CallNode");
    auto &outer = std::get<CallNode>(bld.getExpr(e));
    CHECK(std::holds_alternative<CallNode>(bld.getExpr(outer.callee)), "inner is CallNode");
}

static void test_parse_field_access() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "obj.field");
    CHECK(e != kInvalidExpr, "parsed obj.field");
    CHECK(std::holds_alternative<FieldNode>(bld.getExpr(e)), "obj.field is FieldNode");
    auto &f = std::get<FieldNode>(bld.getExpr(e));
    CHECK(f.field == "field", "field name is 'field'");
    CHECK(std::holds_alternative<IdentNode>(bld.getExpr(f.object)), "object is ident");
}

static void test_parse_chained_field_access() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "a.b.c");
    CHECK(e != kInvalidExpr, "parsed a.b.c");
    CHECK(std::holds_alternative<FieldNode>(bld.getExpr(e)), "a.b.c is FieldNode");
    auto &outer = std::get<FieldNode>(bld.getExpr(e));
    CHECK(outer.field == "c", "outer field is c");
    CHECK(std::holds_alternative<FieldNode>(bld.getExpr(outer.object)), "inner is FieldNode");
    auto &inner = std::get<FieldNode>(bld.getExpr(outer.object));
    CHECK(inner.field == "b", "inner field is b");
}

static void test_parse_index() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "arr[0]");
    CHECK(e != kInvalidExpr, "parsed arr[0]");
    CHECK(std::holds_alternative<IndexNode>(bld.getExpr(e)), "arr[0] is IndexNode");
    auto &idx = std::get<IndexNode>(bld.getExpr(e));
    CHECK(std::holds_alternative<IdentNode>(bld.getExpr(idx.object)), "object is ident");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(idx.index)), "index is literal");
}

static void test_parse_call_then_field() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "get_obj().field");
    CHECK(e != kInvalidExpr, "parsed get_obj().field");
    CHECK(std::holds_alternative<FieldNode>(bld.getExpr(e)), "is FieldNode");
    auto &f = std::get<FieldNode>(bld.getExpr(e));
    CHECK(f.field == "field", "field name");
    CHECK(std::holds_alternative<CallNode>(bld.getExpr(f.object)), "object is CallNode");
}

// ── if expression tests ──────────────────────────────────────────────

static void test_parse_if() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "if true { 1 }");
    CHECK(e != kInvalidExpr, "parsed if expr");
    CHECK(std::holds_alternative<IfNode>(bld.getExpr(e)), "if node is IfNode");
    auto &iff = std::get<IfNode>(bld.getExpr(e));
    CHECK(iff.cond != kInvalidExpr, "if has condition");
    CHECK(iff.then_branch != kInvalidExpr, "if has then branch");
    CHECK(iff.else_branch == kInvalidExpr, "if has no else branch");
}

static void test_parse_if_else() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "if true { 1 } else { 2 }");
    CHECK(e != kInvalidExpr, "parsed if-else expr");
    CHECK(std::holds_alternative<IfNode>(bld.getExpr(e)), "if-else is IfNode");
    auto &iff = std::get<IfNode>(bld.getExpr(e));
    CHECK(iff.cond != kInvalidExpr, "if-else has condition");
    CHECK(iff.then_branch != kInvalidExpr, "if-else has then branch");
    CHECK(iff.else_branch != kInvalidExpr, "if-else has else branch");
}

static void test_parse_if_else_if() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "if a { 1 } else if b { 2 } else { 3 }");
    CHECK(e != kInvalidExpr, "parsed if-else if-else expr");
    CHECK(std::holds_alternative<IfNode>(bld.getExpr(e)), "outer is IfNode");
    auto &outer = std::get<IfNode>(bld.getExpr(e));
    CHECK(outer.else_branch != kInvalidExpr, "outer has else branch");
    CHECK(std::holds_alternative<IfNode>(bld.getExpr(outer.else_branch)),
          "else branch is another IfNode");
}

// ── while loop tests ─────────────────────────────────────────────────

static void test_parse_while() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "while true { 1 }");
    CHECK(e != kInvalidExpr, "parsed while expr");
    CHECK(std::holds_alternative<WhileNode>(bld.getExpr(e)), "while is WhileNode");
    auto &w = std::get<WhileNode>(bld.getExpr(e));
    CHECK(w.cond != kInvalidExpr, "while has condition");
    CHECK(w.body != kInvalidExpr, "while has body");
}

// ── for loop tests (desugars to while) ───────────────────────────────

static void test_parse_for() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "for (let i = 0; i < 10; i + 1) { i }");
    CHECK(e != kInvalidExpr, "parsed for expr");
    // for desugars to a block containing a while
    CHECK(std::holds_alternative<BlockNode>(bld.getExpr(e)), "for desugars to BlockNode");
}

// ── parenthesized expression tests ───────────────────────────────────

static void test_parse_parens() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "(1 + 2) * 3");
    CHECK(e != kInvalidExpr, "parsed (1 + 2) * 3");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(e)), "root is BinaryNode");
    auto &bin = std::get<BinaryNode>(bld.getExpr(e));
    CHECK(bin.op == BinaryOp::Mul, "root op is Mul");
    CHECK(std::holds_alternative<BinaryNode>(bld.getExpr(bin.lhs)), "lhs is BinaryNode (+)");
    auto &add = std::get<BinaryNode>(bld.getExpr(bin.lhs));
    CHECK(add.op == BinaryOp::Add, "inner op is Add");
}

static void test_parse_nested_parens() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto e = parseExprStr(arena, bld, diags, "((42))");
    CHECK(e != kInvalidExpr, "parsed ((42))");
    CHECK(std::holds_alternative<LitValue>(bld.getExpr(e)), "nested parens yield inner literal");
}

// ── statement parsing tests ──────────────────────────────────────────

static void test_parse_let_stmt() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test", std::string("let x = 42"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto s = p.parseStmt();
    CHECK(s != kInvalidStmt, "parsed let statement");
    CHECK(std::holds_alternative<LetNode>(bld.getStmt(s)), "let stmt is LetNode");
    auto &let = std::get<LetNode>(bld.getStmt(s));
    CHECK(let.names[0] == "x", "let name is x");
    CHECK(!let.mut, "let is not mutable");
    CHECK(let.init != kInvalidExpr, "let has initializer");
}

static void test_parse_let_stmt_no_init() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test", std::string("let x"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto s = p.parseStmt();
    CHECK(s != kInvalidStmt, "parsed let statement without init");
    auto &let = std::get<LetNode>(bld.getStmt(s));
    CHECK(let.names[0] == "x", "let name is x");
    CHECK(let.init == kInvalidExpr, "let has no initializer");
}

static void test_parse_return_stmt() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test", std::string("return 42"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto s = p.parseStmt();
    CHECK(s != kInvalidStmt, "parsed return statement");
    CHECK(std::holds_alternative<RetNode>(bld.getStmt(s)), "return is RetNode");
    auto &ret = std::get<RetNode>(bld.getStmt(s));
    CHECK(ret.value != kInvalidExpr, "return has value");
}

static void test_parse_return_void() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test", std::string("return"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto s = p.parseStmt();
    CHECK(s != kInvalidStmt, "parsed bare return");
    auto &ret = std::get<RetNode>(bld.getStmt(s));
    CHECK(ret.value == kInvalidExpr, "bare return has no value");
}

static void test_parse_assign_stmt() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test", std::string("x = 5"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto s = p.parseStmt();
    CHECK(s != kInvalidStmt, "parsed expression statement (assignment TBD)");
}

// ── block parsing tests ──────────────────────────────────────────────

static void test_parse_block() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test", std::string("{ let x = 1; x }"), diags)
            .value();
    Parser p(&toks, &bld, &diags);
    auto e = p.parseBlock();
    CHECK(e != kInvalidExpr, "parsed block");
    CHECK(std::holds_alternative<BlockNode>(bld.getExpr(e)), "block is BlockNode");
    auto &blk = std::get<BlockNode>(bld.getExpr(e));
    CHECK(blk.stmts.size() >= 1, "block has at least 1 stmt");
}

// ── error recovery tests ─────────────────────────────────────────────

static void test_parse_error_recovery_junk() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks = tokenize(parser_full_source_map, arena, "test", std::string("42;"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto result = zith::parser::parseProgram(toks, bld, diags);
    CHECK(diags.hasErrors(), "junk input produces errors");
}

static void test_parse_error_missing_semicolon() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    // parseStmt should recover from missing ';' after expression
    auto toks = tokenize(parser_full_source_map, arena, "test", std::string("42"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto s = p.parseStmt();
    CHECK(s != kInvalidStmt, "expression stmt without semicolon still produces stmt");
}

static void test_parse_error_in_let() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test", std::string("let 42 = 1"), diags).value();
    Parser p(&toks, &bld, &diags);
    auto s = p.parseStmt();
    CHECK(diags.hasErrors(), "let with invalid name produces errors");
}

// ── scan + expand body tests ─────────────────────────────────────────

static void test_scan_fn_body() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto toks = tokenize(parser_full_source_map, arena, "test",
                         std::string("fn foo() { return 1; }"), diags)
                    .value();
    Parser parser(&toks, &bld, &diags);
    auto result = scan(parser, syms);

    CHECK_EQ(result.fns.size(), size_t(1), "one fn scanned");
    CHECK(result.fns[0].name == "foo", "fn name is foo");
    CHECK(std::holds_alternative<UnbodyNode>(bld.getExpr(result.fns[0].body_node)),
          "body is UnbodyNode after scan");

    parser.expandBodies(result);
    CHECK(!std::holds_alternative<UnbodyNode>(bld.getExpr(result.fns[0].body_node)),
          "body is no longer UnbodyNode after expand");
    CHECK(std::holds_alternative<BlockNode>(bld.getExpr(result.fns[0].body_node)),
          "body is BlockNode after expand");
}

static void test_scan_struct_body() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto toks = tokenize(parser_full_source_map, arena, "test",
                         std::string("struct Point { x: i32, y: f64 }"), diags)
                    .value();
    Parser parser(&toks, &bld, &diags);
    auto result = scan(parser, syms);

    CHECK_EQ(result.structs.size(), size_t(1), "one struct scanned");
    CHECK(result.structs[0].name == "Point", "struct name is Point");
    CHECK(result.structs[0].body_node != kInvalidExpr, "struct has body node");
    CHECK(std::holds_alternative<UnbodyNode>(bld.getExpr(result.structs[0].body_node)),
          "struct body is UnbodyNode after scan");

    parser.expandBodies(result);
    CHECK(!diags.hasErrors(), "struct body expansion produces no errors");
}

static void test_scan_struct_with_methods() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto toks =
        tokenize(parser_full_source_map, arena, "test",
                 std::string("struct Foo { data: i32, fn get() -> i32 { return data; } }"), diags)
            .value();
    Parser parser(&toks, &bld, &diags);
    auto result = scan(parser, syms);

    CHECK_EQ(result.structs.size(), size_t(1), "one struct scanned");
    // Methods inside struct bodies are not extracted as separate scan entries
    CHECK(parser.program.decls.size() >= 1, "at least one decl in program");

    parser.expandBodies(result);
    CHECK(!diags.hasErrors(), "struct with method expansion produces no errors");
}

static void test_scan_multiple_fns() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto toks = tokenize(parser_full_source_map, arena, "test",
                         std::string("fn a() {} fn b() { return 1; } fn c(x) { x }"), diags)
                    .value();
    Parser parser(&toks, &bld, &diags);
    auto result = scan(parser, syms);

    CHECK_EQ(result.fns.size(), size_t(3), "three fns scanned");
    CHECK(result.fns[0].name == "a", "fn[0] name is a");
    CHECK(result.fns[1].name == "b", "fn[1] name is b");
    CHECK(result.fns[2].name == "c", "fn[2] name is c");
}

static void test_parse_program_with_fns() {
    Arena arena;
    AstBuilder bld(arena);
    DiagnosticEngine diags(arena);

    auto toks = tokenize(parser_full_source_map, arena, "test",
                         std::string("fn foo(x) { return x; } fn bar() { foo(1); }"), diags)
                    .value();
    auto prog_result = zith::parser::parseProgram(toks, bld, diags);
    CHECK(prog_result.isOk(), "parseProgram returns value");
    auto &program = prog_result.value();
    CHECK_EQ(program.decls.size(), size_t(2), "program has 2 decls");
    CHECK(std::holds_alternative<FnDeclNode>(bld.getDecl(program.decls[0])),
          "decl 0 is FnDeclNode");
    CHECK(std::holds_alternative<FnDeclNode>(bld.getDecl(program.decls[1])),
          "decl 1 is FnDeclNode");
}

// ── main ─────────────────────────────────────────────────────────────

int main() {
    std::printf("parser-full tests\n");
    std::printf("=====================\n\n");

    // Binary operators
    test_parse_binary_add();
    test_parse_binary_sub();
    test_parse_binary_mul();
    test_parse_binary_div();
    test_parse_binary_rest();
    test_parse_binary_mul_precedence();
    test_parse_binary_add_precedence();
    test_parse_binary_chained();

    // Compound operators
    test_parse_compound_eq();
    test_parse_compound_ne();
    test_parse_compound_le();
    test_parse_compound_ge();
    test_parse_compound_shl();
    test_parse_compound_shr();
    test_parse_compound_and();
    test_parse_compound_or();

    // Word operators
    test_parse_word_and();
    test_parse_word_or();
    test_parse_word_xor();

    // Unary prefix operators
    test_parse_unary_neg();
    test_parse_unary_not();
    test_parse_unary_ref();
    test_parse_unary_deref();
    test_parse_unary_word_not();

    // Call, field, index
    test_parse_call_no_args();
    test_parse_call_one_arg();
    test_parse_call_two_args();
    test_parse_call_chained();
    test_parse_field_access();
    test_parse_chained_field_access();
    test_parse_index();
    test_parse_call_then_field();

    // If expressions
    test_parse_if();
    test_parse_if_else();
    test_parse_if_else_if();

    // While/for loops
    test_parse_while();
    test_parse_for();

    // Parenthesized expressions
    test_parse_parens();
    test_parse_nested_parens();

    // Statement parsing
    test_parse_let_stmt();
    test_parse_let_stmt_no_init();
    test_parse_return_stmt();
    test_parse_return_void();
    test_parse_assign_stmt();
    test_parse_block();

    // Error recovery
    test_parse_error_recovery_junk();
    test_parse_error_missing_semicolon();
    test_parse_error_in_let();

    // Scan + expand bodies
    test_scan_fn_body();
    test_scan_struct_body();
    test_scan_struct_with_methods();
    test_scan_multiple_fns();
    test_parse_program_with_fns();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
