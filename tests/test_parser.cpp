#include "../include/zith/parser.h"
#include "zith/ast.h"
#include "zith/parser.h"
#include "zith/parser.hpp"
#include "test.h"
#include <string>

TEST_CASE("SCAN: empty source produces a PROGRAM node", "[scan]") {
    auto ast = parse_test("");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);
    REQUIRE(ast->data.list.len == 0);
}

TEST_CASE("SCAN: single function declaration", "[scan]") {
    auto ast = parse_test("fn foo(): i32 { return 42; }");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);

    auto *p = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p != nullptr);
    REQUIRE(std::string(p->name) == "foo");
    REQUIRE(p->param_count == 0);
}

TEST_CASE("SCAN: function with parameters", "[scan]") {
    auto ast = parse_test("fn add(a, b, c) -> i32 { return a + b; }");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    auto *p    = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p->param_count == 3);

    auto *pa = static_cast<ZithParamPayload *>(p->params[0]->data.list.ptr);
    REQUIRE(pa->name[0] == 'a');
    auto *pb = static_cast<ZithParamPayload *>(p->params[1]->data.list.ptr);
    REQUIRE(pb->name[0] == 'b');
}

TEST_CASE("SCAN: function body is UNBODY", "[scan]") {
    auto ast = parse_test("fn foo() { let x = 1; }");
    REQUIRE(ast);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    auto *p    = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p->body != nullptr);
    REQUIRE(p->body->type == ZITH_NODE_UNBODY);
    REQUIRE(p->body->data.list.len > 0);
}

TEST_CASE("SCAN: struct declaration", "[scan]") {
    auto ast = parse_test("struct Point {\n"
                          "    x: i32,\n"
                          "    y: i32,\n"
                          "}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_STRUCT_DECL);

    auto *p = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    REQUIRE(std::string(p->name) == "Point");
    REQUIRE(p->field_count == 2);
}

TEST_CASE("SCAN: multiple declarations", "[scan]") {
    auto ast = parse_test("fn foo() {}\n"
                          "fn bar() {}\n"
                          "fn baz() {}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 3);
}

TEST_CASE("SCAN: async fn kind", "[scan][fn-kind]") {
    auto ast = parse_test("async fn fetch() { }");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);

    auto *p = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p != nullptr);
    REQUIRE(std::string(p->name) == "fetch");
    REQUIRE(p->kind == ZITH_FN_ASYNC);
}

TEST_CASE("SCAN: noreturn fn kind", "[scan][fn-kind]") {
    auto ast = parse_test("noreturn fn crash() { }");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);

    auto *p = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p != nullptr);
    REQUIRE(std::string(p->name) == "crash");
    REQUIRE(p->kind == ZITH_FN_NORETURN);
}

TEST_CASE("SCAN: flowing fn kind", "[scan][fn-kind]") {
    auto ast = parse_test("flowing fn handler() { }");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);

    auto *p = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p != nullptr);
    REQUIRE(std::string(p->name) == "handler");
    REQUIRE(p->kind == ZITH_FN_FLOWING);
}

TEST_CASE("SCAN: public visibility", "[scan]") {
    auto ast = parse_test("pub fn init() { }");
    REQUIRE(ast);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    auto *p    = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p->visibility == ZITH_VIS_PUBLIC);
}

TEST_CASE("SCAN: struct with method", "[scan]") {
    auto ast = parse_test("struct Vec {\n"
                          "    x: i32,\n"
                          "    fn len() -> i32 { }\n"
                          "}");
    REQUIRE(ast);

    auto *sdecl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    auto *sp    = static_cast<ZithStructPayload *>(sdecl->data.list.ptr);
    REQUIRE(sp->field_count == 1);
    REQUIRE(sp->method_count == 1);
}

TEST_MAIN()
