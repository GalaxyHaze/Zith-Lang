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
