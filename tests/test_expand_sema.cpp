#include <catch2/catch_test_macros.hpp>

#include "zith/ast.h"
#include "zith/parser.h"
#include "zith/parser.hpp"

TEST_CASE("FULL: function body is expanded into BLOCK", "[full][expand]") {
    auto ast = ParseResult(zith_parse_test_full("fn main() -> i32 {\n"
                                                "  let v: i32 = 1;\n"
                                                "  if (v < 2) { return 10; }\n"
                                                "  for (v < 0) { return 20; }\n"
                                                "  return 30;\n"
                                                "}"));

    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);
    REQUIRE(ast->data.list.len == 1);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    auto *fn     = static_cast<ZithFuncPayload *>(decls[0]->data.list.ptr);
    REQUIRE(fn->body != nullptr);
    REQUIRE(fn->body->type == ZITH_NODE_BLOCK);

    auto **stmts = static_cast<ZithNode **>(fn->body->data.list.ptr);
    REQUIRE(fn->body->data.list.len >= 3);
    REQUIRE(stmts[0]->type == ZITH_NODE_VAR_DECL);
    REQUIRE(stmts[1]->type == ZITH_NODE_IF);
    REQUIRE(stmts[2]->type == ZITH_NODE_FOR);
}

TEST_CASE("FULL: optional/failable assignment is consistent", "[full][sema][types]") {
    auto ok = ParseResult(zith_parse_test_full("fn main() {\n"
                                               "  let x: i32 = 7;\n"
                                               "  let maybe: ?i32 = x;\n"
                                               "  let risky: !i32 = x;\n"
                                               "  return;\n"
                                               "}"));
    REQUIRE(ok);

    auto bad = ParseResult(zith_parse_test_full("fn main() {\n"
                                                "  let risky: i32! = 7;\n"
                                                "  let plain: i32 = risky;\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: import alias resolution is minimally stable", "[full][sema][module]") {
    auto ast = ParseResult(zith_parse_test_full("import std.io.console as io;\n"
                                                "fn main() {\n"
                                                "  io.println(\"ok\");\n"
                                                "}\n"));

    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);
    REQUIRE(ast->data.list.len == 2);
}

TEST_CASE("FULL: return type mismatch fails semantic phase", "[full][sema][return]") {
    auto ast = ParseResult(zith_parse_test_full("fn main() -> i32 {\n"
                                                "  return \"oops\";\n"
                                                "}\n"));
    REQUIRE(ast.get() == nullptr);
}

// ============================================================================
// Typed parameters (regression: parser_check vs parser_match in parse_param)
// ============================================================================

TEST_CASE("FULL: typed function parameters are accepted", "[full][sema][params]") {
    auto ast = ParseResult(zith_parse_test_full("fn add(a: i32, b: i32) -> i32 {\n"
                                                "  return a + b;\n"
                                                "}\n"));
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);
    REQUIRE(ast->data.list.len == 1);
}

TEST_CASE("FULL: mixed typed and untyped parameters", "[full][sema][params]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: i32, y) -> i32 {\n"
                                                "  return x;\n"
                                                "}\n"));
    REQUIRE(ast);
}

// ============================================================================
// Function overloading
// ============================================================================

TEST_CASE("FULL: overloaded functions with different param counts", "[full][sema][overload]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo() -> i32 { return 0; }\n"
                                                "fn foo(x: i32) -> i32 { return x; }\n"
                                                "fn foo(x: i32, y: i32) -> i32 { return x + y; }\n"
                                                "fn main() { return; }\n"));
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 4);
}

TEST_CASE("FULL: overloaded functions with different param types", "[full][sema][overload]") {
    auto ast = ParseResult(zith_parse_test_full("fn bar(x: i32) -> i32 { return x; }\n"
                                                "fn bar(x: string) -> string { return x; }\n"
                                                "fn main() { return; }\n"));
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 3);
}

TEST_CASE("FULL: duplicate function with same signature is rejected", "[full][sema][overload]") {
    auto bad = ParseResult(zith_parse_test_full("fn dup(a: i32) -> i32 { return a; }\n"
                                                "fn dup(b: i32) -> i32 { return b; }\n"
                                                "fn main() { return; }\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: overloaded function call resolves to correct overload", "[full][sema][overload]") {
    auto ast = ParseResult(zith_parse_test_full("fn add(x: i32, y: i32) -> i32 { return x + y; }\n"
                                                "fn add(x: string, y: string) -> string { return x; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return add(1, 2);\n"
                                                "}\n"));
    REQUIRE(ast);
}

TEST_CASE("FULL: call with no matching overload is rejected", "[full][sema][overload]") {
    auto bad = ParseResult(zith_parse_test_full("fn compute(x: i32) -> i32 { return x; }\n"
                                                "fn compute(x: i32, y: i32) -> i32 { return x + y; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return compute(true);\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: too few arguments to single overload is rejected", "[full][sema][overload]") {
    auto bad = ParseResult(zith_parse_test_full("fn need_two(a: i32, b: i32) -> i32 { return a + b; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return need_two(1);\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: too many arguments to single overload is rejected", "[full][sema][overload]") {
    auto bad = ParseResult(zith_parse_test_full("fn one_arg(x: i32) -> i32 { return x; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return one_arg(1, 2, 3);\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: correct argument count to single overload passes", "[full][sema][overload]") {
    auto ast = ParseResult(zith_parse_test_full("fn single(x: i32) -> i32 { return x; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return single(42);\n"
                                                "}\n"));
    REQUIRE(ast);
}

TEST_CASE("FULL: wrong arity for all overloads is rejected", "[full][sema][overload]") {
    auto bad = ParseResult(zith_parse_test_full("fn f(x: i32) -> i32 { return x; }\n"
                                                "fn f(x: i32, y: i32) -> i32 { return x + y; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return f(1, 2, 3);\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: member call too few arguments is rejected", "[full][sema][overload]") {
    auto bad = ParseResult(zith_parse_test_full("import std.io.console as io;\n"
                                                "fn parse(x: i32) -> i32 { return x; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return io.parse();\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: member call too many arguments is rejected", "[full][sema][overload]") {
    auto bad = ParseResult(zith_parse_test_full("import std.io.console as io;\n"
                                                "fn parse(x: i32) -> i32 { return x; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return io.parse(1, 2, 3);\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: member call correct argument count passes", "[full][sema][overload]") {
    auto ast = ParseResult(zith_parse_test_full("import std.io.console as io;\n"
                                                "fn parse(x: i32) -> i32 { return x; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return io.parse(42);\n"
                                                "}\n"));
    REQUIRE(ast);
}

// ============================================================================
// :: scope operator
// ============================================================================

TEST_CASE("FULL: ::expr resolves from outer scope", "[full][sema][scope]") {
    auto ast = ParseResult(zith_parse_test_full("fn main() -> i32 {\n"
                                                "  let x: i32 = 1;\n"
                                                "  {\n"
                                                "    let _: i32 = ::x;\n"
                                                "  }\n"
                                                "  return 0;\n"
                                                "}\n"));
    REQUIRE(ast);
}



// ============================================================================
// Visibility — mod fn passes sema
// ============================================================================

TEST_CASE("FULL: mod visibility function compiles", "[full][sema][visibility]") {
    auto ast = ParseResult(zith_parse_test_full("mod fn helper() -> i32 { return 42; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return helper();\n"
                                                "}\n"));
    REQUIRE(ast);
}

TEST_CASE("FULL: mod(3) visibility function compiles", "[full][sema][visibility]") {
    auto ast = ParseResult(zith_parse_test_full("mod(3) fn helper() -> i32 { return 7; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return helper();\n"
                                                "}\n"));
    REQUIRE(ast);
}

TEST_CASE("FULL: pub: default applies to subsequent functions", "[full][sema][visibility]") {
    auto ast = ParseResult(zith_parse_test_full("pub:\n"
                                                "fn a() -> i32 { return 1; }\n"
                                                "fn b() -> i32 { return 2; }\n"
                                                "fn main() -> i32 {\n"
                                                "  return a() + b();\n"
                                                "}\n"));
    REQUIRE(ast);
}

// ============================================================================
// Ownership extraction from type annotations
// ============================================================================

TEST_CASE("FULL: param with view ownership is accepted", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: view i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(view x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: param with lend ownership is accepted", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: lend i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(lend x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: param with extension ownership is accepted", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: extension i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(extension x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: param with share ownership is accepted", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: share i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(share x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: param with unique ownership is accepted", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: unique i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(unique x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: variable with view type annotation", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn main() { let x: view i32 = 1; }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: optional with ownership view ?i32", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: view ?i32) { }\n"
                                                "fn main() { foo(view 1); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: ownership with optional view ?i32", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: ?view i32) { }\n"
                                                "fn main() { foo(view 1); }"));
    REQUIRE(ast);
}

// ============================================================================
// Ownership validation at call sites
// ============================================================================

TEST_CASE("FULL: view param without annotation is error", "[full][sema][ownership][error]") {
    auto bad = ParseResult(zith_parse_test_full("fn foo(x: view i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(x); }"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: lend param without annotation is error", "[full][sema][ownership][error]") {
    auto bad = ParseResult(zith_parse_test_full("fn foo(x: lend i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(x); }"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: extension param without annotation is error", "[full][sema][ownership][error]") {
    auto bad = ParseResult(zith_parse_test_full("fn foo(x: extension i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(x); }"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: share param without annotation, source not shared is error", "[full][sema][ownership][error]") {
    auto bad = ParseResult(zith_parse_test_full("fn foo(x: share i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(x); }"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("FULL: view param with view annotation is OK", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: view i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(view x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: lend param with lend annotation is OK", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: lend i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(lend x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: extension param with extension annotation is OK", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: extension i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(extension x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: share param with share annotation is OK", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: share i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(share x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: literal arg to view param is OK", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: view i32) { }\n"
                                                "fn main() { foo(view 42); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: mixed annotations in multi-arg call", "[full][sema][ownership]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(a: view i32, b: i32, c: lend i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(view x, 42, lend x); }"));
    REQUIRE(ast);
}

TEST_CASE("FULL: wrong annotation type is error", "[full][sema][ownership][error]") {
    auto bad = ParseResult(zith_parse_test_full("fn foo(x: view i32) { }\n"
                                                "fn main() { let x: i32 = 1; foo(lend x); }"));
    REQUIRE(bad.get() == nullptr);
}

// ============================================================================
// Expected-to-fail: struct type checking (not yet implemented)
// ============================================================================

TEST_CASE("TODO: non-existent struct field access is rejected", "[full][sema][struct][!shouldfail]") {
    auto bad = ParseResult(zith_parse_test_full("struct Point { x: i32, y: i32 }\n"
                                                "fn main() -> i32 {\n"
                                                "  let p: Point = Point{x: 1, y: 2};\n"
                                                "  return p.z;\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("TODO: struct field type mismatch is rejected", "[full][sema][struct][!shouldfail]") {
    auto bad = ParseResult(zith_parse_test_full("struct Point { x: i32, y: i32 }\n"
                                                "fn main() {\n"
                                                "  let p: Point = Point{x: \"hello\", y: 2};\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("TODO: undefined struct type is rejected", "[full][sema][struct][!shouldfail]") {
    auto bad = ParseResult(zith_parse_test_full("fn main() {\n"
                                                "  let p: UndefinedStruct = x;\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

TEST_CASE("TODO: method call on struct validates receiver type", "[full][sema][struct][!shouldfail]") {
    auto bad = ParseResult(zith_parse_test_full("struct Point { x: i32, y: i32 }\n"
                                                "fn distance(self: Point) -> i32 { return self.x; }\n"
                                                "fn main() -> i32 {\n"
                                                "  let p: Point = Point{x: 1, y: 2};\n"
                                                "  return p.distance();\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

// ============================================================================
// Expected-to-fail: ownership move semantics (not yet implemented)
// ============================================================================

TEST_CASE("TODO: use of moved unique variable is rejected", "[full][sema][ownership][!shouldfail]") {
    auto bad = ParseResult(zith_parse_test_full("fn main() {\n"
                                                "  let x: unique i32 = 1;\n"
                                                "  let y = x;\n"
                                                "  let z = x;\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}

// ============================================================================
// Member access type propagation (Phase 3 placeholder)
// ============================================================================

TEST_CASE("FULL: member access propagates type", "[full][sema][member]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: i32) { }\n"
                                                "fn main() {\n"
                                                "  let x: i32 = 1;\n"
                                                "  foo(x.field);\n"
                                                "}\n"));
    REQUIRE(ast);
}

TEST_CASE("FULL: chained member access propagates type", "[full][sema][member]") {
    auto ast = ParseResult(zith_parse_test_full("fn foo(x: i32) { }\n"
                                                "fn main() {\n"
                                                "  let x: i32 = 1;\n"
                                                "  foo(x.a.b.c);\n"
                                                "}\n"));
    REQUIRE(ast);
}

TEST_CASE("TODO: use-after-move in function call is rejected", "[full][sema][ownership][!shouldfail]") {
    auto bad = ParseResult(zith_parse_test_full("fn consume(x: unique i32) { }\n"
                                                "fn main() {\n"
                                                "  let x: unique i32 = 1;\n"
                                                "  consume(x);\n"
                                                "  consume(x);\n"
                                                "}\n"));
    REQUIRE(bad.get() == nullptr);
}
