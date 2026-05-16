#include "../include/zith/parser.h"
#include "zith/ast.h"
#include "zith/parser.h"
#include "zith/parser.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

// ============================================================================
// SCAN Phase — declarations and signatures
// ============================================================================

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

// ============================================================================
// Function Kinds
// ============================================================================

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
    auto ast = parse_test("public fn init() { }");
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

    auto *m  = sp->methods[0];
    auto *mp = static_cast<ZithFuncPayload *>(m->data.list.ptr);
    REQUIRE(std::string(mp->name) == "len");
}

// ============================================================================
// Import / Export / From
// ============================================================================

TEST_CASE("SCAN: import declaration", "[scan][module]") {
    auto ast = parse_test("import std.io;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io");
    REQUIRE(payload->is_export == false);
    REQUIRE(payload->is_from == false);
}

TEST_CASE("SCAN: import declaration with slash separator", "[scan][module]") {
    auto ast = parse_test("import std/io;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std/io");
    REQUIRE(payload->is_export == false);
    REQUIRE(payload->is_from == false);
}

TEST_CASE("SCAN: import declaration with alias", "[scan][module]") {
    auto ast = parse_test("import std.io.console as console;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io.console");
    REQUIRE(payload->alias != nullptr);
    REQUIRE(std::string(payload->alias, payload->alias_len) == "console");
}

TEST_CASE("SCAN: import declaration with mixed separators", "[scan][module]") {
    auto ast = parse_test("import std/io/console;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std/io/console");
    REQUIRE(payload->is_export == false);
    REQUIRE(payload->is_from == false);
}

TEST_CASE("SCAN: import with directory recursion (flat, default)", "[scan][module][directory]") {
    auto ast = parse_test("import std.io;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io");
    REQUIRE(payload->recurse_depth == 0);
}

TEST_CASE("SCAN: import with directory recursion (infinite)", "[scan][module][directory]") {
    auto ast = parse_test("import std.io (...);");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io");
    REQUIRE(payload->recurse_depth == -1);
}

TEST_CASE("SCAN: import with directory recursion (N levels)", "[scan][module][directory]") {
    auto ast = parse_test("import std.io (5);");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io");
    REQUIRE(payload->recurse_depth == 5);
}

TEST_CASE("SCAN: from declaration", "[scan][module]") {
    auto ast = parse_test("from std.io.console;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io.console");
    REQUIRE(payload->is_from == true);
    REQUIRE(payload->alias == nullptr);
}

TEST_CASE("SCAN: from with slash separator", "[scan][module]") {
    auto ast = parse_test("from std/io/console;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std/io/console");
    REQUIRE(payload->is_from == true);
    REQUIRE(payload->alias == nullptr);
}

TEST_CASE("SCAN: from with mixed path separators", "[scan][module]") {
    auto ast = parse_test("from std.io/net/http;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io/net/http");
    REQUIRE(payload->is_from == true);
    REQUIRE(payload->alias == nullptr);
}

TEST_CASE("SCAN: from with directory recursion (infinite)", "[scan][module][directory]") {
    auto ast = parse_test("from std.io (...);");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io");
    REQUIRE(payload->is_from == true);
    REQUIRE(payload->recurse_depth == -1);
}

TEST_CASE("SCAN: from with directory recursion (N levels)", "[scan][module][directory]") {
    auto ast = parse_test("from std.io (3);");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *import_node = decls[0];
    REQUIRE(import_node->type == ZITH_NODE_IMPORT);

    auto *payload = static_cast<ZithImportPayload *>(import_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io");
    REQUIRE(payload->is_from == true);
    REQUIRE(payload->recurse_depth == 3);
}

TEST_CASE("SCAN: export declaration", "[scan][module]") {
    auto ast = parse_test("export std.io.console.println;");
    REQUIRE(ast);
    REQUIRE(ast->type == ZITH_NODE_PROGRAM);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len >= 1);

    auto *export_node = decls[0];
    REQUIRE(export_node->type == ZITH_NODE_EXPORT);

    auto *payload = static_cast<ZithImportPayload *>(export_node->data.list.ptr);
    REQUIRE(payload != nullptr);
    REQUIRE(std::string(payload->path) == "std.io.console.println");
    REQUIRE(payload->is_export == true);
    REQUIRE(payload->vis == ZITH_VIS_PUBLIC);
}

TEST_CASE("SCAN: mixed declarations with import, export, and functions", "[scan][module]") {
    auto ast = parse_test("import std.io;\n"
                          "export std.io.println;\n"
                          "fn main() { }\n");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 3);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(decls[0]->type == ZITH_NODE_IMPORT);
    REQUIRE(decls[1]->type == ZITH_NODE_EXPORT);
    REQUIRE(decls[2]->type == ZITH_NODE_FUNC_DECL);
}

// ============================================================================
// Error handling - Import/Export/From
// ============================================================================

TEST_CASE("SCAN: import without semicolon", "[scan][error][module]") {
    auto ast = parse_test("import std.io");
    (void)ast;
}

TEST_CASE("SCAN: import without module path", "[scan][error][module]") {
    auto ast = parse_test("import ;");
    (void)ast;
}

TEST_CASE("SCAN: export without path", "[scan][error][module]") {
    auto ast = parse_test("export ;");
    (void)ast;
}

TEST_CASE("SCAN: export without semicolon", "[scan][error][module]") {
    auto ast = parse_test("export std.io.console.println");
    (void)ast;
}

TEST_CASE("SCAN: from without semicolon", "[scan][error][module]") {
    auto ast = parse_test("from std.io.console");
    (void)ast;
}

TEST_CASE("SCAN: import with double dot", "[scan][error][module]") {
    auto ast = parse_test("import std..io;");
    (void)ast;
}

TEST_CASE("SCAN: from with double dot", "[scan][error][module]") {
    auto ast = parse_test("from std..io;");
    (void)ast;
}

TEST_CASE("SCAN: export with double dot", "[scan][error][module]") {
    auto ast = parse_test("export std..io.console;");
    (void)ast;
}

TEST_CASE("SCAN: import alias without name", "[scan][error][module]") {
    auto ast = parse_test("import std.io as ;");
    (void)ast;
}

// ============================================================================
// Concurrency keywords
// ============================================================================

TEST_CASE("SCAN: spawn keyword tokenized", "[scan][lexer][concurrency]") {
    auto ast = parse_test("spawn fn worker() { }");
    // At minimum should tokenize without error
    (void)ast;
}

TEST_CASE("SCAN: await keyword tokenized", "[scan][lexer][concurrency]") {
    auto ast = parse_test("fn main() { await task; }");
    (void)ast;
}

TEST_CASE("SCAN: join keyword tokenized", "[scan][lexer][concurrency]") {
    auto ast = parse_test("fn main() { join handle; }");
    (void)ast;
}

// ============================================================================
// Error handling keywords
// ============================================================================

TEST_CASE("SCAN: try keyword tokenized", "[scan][lexer][errors]") {
    auto ast = parse_test("fn main() { try operation; }");
    (void)ast;
}

TEST_CASE("SCAN: catch keyword tokenized", "[scan][lexer][errors]") {
    auto ast = parse_test("fn main() { catch { Err -> } }");
    (void)ast;
}

TEST_CASE("SCAN: throw keyword tokenized", "[scan][lexer][errors]") {
    auto ast = parse_test("fn main() { throw Error; }");
    (void)ast;
}

TEST_CASE("SCAN: must keyword tokenized", "[scan][lexer][errors]") {
    auto ast = parse_test("fn main() { must operation(); }");
    (void)ast;
}

TEST_CASE("SCAN: do keyword tokenized", "[scan][lexer][errors]") {
    auto ast = parse_test("do { init(); }");
    (void)ast;
}

TEST_CASE("SCAN: drop keyword tokenized", "[scan][lexer][errors]") {
    auto ast = parse_test("drop { cleanup(); }");
    (void)ast;
}

// ============================================================================
// Ownership keywords
// ============================================================================

TEST_CASE("SCAN: unique keyword tokenized", "[scan][lexer][ownership]") {
    auto ast = parse_test("fn main() { unique x = 5; }");
    (void)ast;
}

TEST_CASE("SCAN: share keyword tokenized", "[scan][lexer][ownership]") {
    auto ast = parse_test("fn main() { share x = 5; }");
    (void)ast;
}

TEST_CASE("SCAN: view keyword tokenized", "[scan][lexer][ownership]") {
    auto ast = parse_test("fn main() { view x = 5; }");
    (void)ast;
}

TEST_CASE("SCAN: lend keyword tokenized", "[scan][lexer][ownership]") {
    auto ast = parse_test("fn main() { lend x = 5; }");
    (void)ast;
}

TEST_CASE("SCAN: extension keyword tokenized", "[scan][lexer][ownership]") {
    auto ast = parse_test("fn main() { extension x = 5; }");
    (void)ast;
}

// ============================================================================
// Metaprogramming keywords
// ============================================================================

TEST_CASE("SCAN: require keyword tokenized", "[scan][lexer][meta]") {
    auto ast = parse_test("require SomeTrait<T>;");
    (void)ast;
}

TEST_CASE("SCAN: is keyword tokenized", "[scan][lexer][meta]") {
    auto ast = parse_test("fn main() { x is T; }");
    (void)ast;
}

TEST_CASE("SCAN: prefix keyword tokenized", "[scan][lexer][meta]") {
    auto ast = parse_test("prefix fn op() { }");
    (void)ast;
}

TEST_CASE("SCAN: sufix keyword tokenized", "[scan][lexer][meta]") {
    auto ast = parse_test("sufix fn op() { }");
    (void)ast;
}

TEST_CASE("SCAN: infix keyword tokenized", "[scan][lexer][meta]") {
    auto ast = parse_test("infix fn op() { }");
    (void)ast;
}

// ============================================================================
// Control flow keywords
// ============================================================================

TEST_CASE("SCAN: marker keyword tokenized", "[scan][lexer][control]") {
    auto ast = parse_test("marker start() { }");
    (void)ast;
}

TEST_CASE("SCAN: entry keyword tokenized", "[scan][lexer][control]") {
    auto ast = parse_test("entry main { }");
    (void)ast;
}

TEST_CASE("SCAN: scene keyword tokenized", "[scan][lexer][control]") {
    auto ast = parse_test("scene game_loop() { }");
    (void)ast;
}

TEST_CASE("SCAN: goto keyword tokenized", "[scan][lexer][control]") {
    auto ast = parse_test("fn main() { goto label; }");
    (void)ast;
}

TEST_CASE("SCAN: recurse keyword tokenized", "[scan][lexer][control]") {
    auto ast = parse_test("fn fib(n) { recurse(n - 1); }");
    (void)ast;
}

// ============================================================================
// Type keywords
// ============================================================================

TEST_CASE("SCAN: trait keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("trait Clone { }");
    (void)ast;
}

TEST_CASE("SCAN: component keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("component Position { }");
    (void)ast;
}

TEST_CASE("SCAN: enum keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("enum Color { Red, Green, Blue }");
    (void)ast;
}

TEST_CASE("SCAN: union keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("union Value { Int, Str }");
    (void)ast;
}

TEST_CASE("SCAN: raw keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("raw block { }");
    (void)ast;
}

TEST_CASE("SCAN: raw function", "[scan][lexer][types]") {
    auto ast = parse_test("raw fn add();");
    (void)ast;
}

TEST_CASE("SCAN: family keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("family Numbers { }");
    (void)ast;
}

TEST_CASE("SCAN: entity keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("entity Player { }");
    (void)ast;
}

TEST_CASE("SCAN: implement keyword tokenized", "[scan][lexer][types]") {
    auto ast = parse_test("implement Clone for MyType { }");
    (void)ast;
}

// ============================================================================
// Binding keywords
// ============================================================================

TEST_CASE("SCAN: let keyword tokenized", "[scan][lexer][bindings]") {
    auto ast = parse_test("fn main() { let x = 5; }");
    (void)ast;
}

TEST_CASE("SCAN: var keyword tokenized", "[scan][lexer][bindings]") {
    auto ast = parse_test("fn main() { var x = 5; }");
    (void)ast;
}

TEST_CASE("SCAN: const keyword tokenized", "[scan][lexer][bindings]") {
    auto ast = parse_test("const PI = 3.14159;");
    (void)ast;
}

TEST_CASE("SCAN: const function", "[scan][lexer][bindings]") {
    auto ast = parse_test("const fn add();");
    (void)ast;
}

TEST_CASE("SCAN: auto keyword tokenized", "[scan][lexer][bindings]") {
    auto ast = parse_test("fn main() { auto x = 5; }");
    (void)ast;
}

TEST_CASE("SCAN: global keyword tokenized", "[scan][lexer][bindings]") {
    auto ast = parse_test("global counter = 0;");
    (void)ast;
}

// ============================================================================
// Operators
// ============================================================================

TEST_CASE("SCAN: and operator tokenized", "[scan][lexer][operators]") {
    auto ast = parse_test("fn main() { a and b; }");
    (void)ast;
}

TEST_CASE("SCAN: or operator tokenized", "[scan][lexer][operators]") {
    auto ast = parse_test("fn main() { a or b; }");
    (void)ast;
}

TEST_CASE("SCAN: arrow operator tokenized", "[scan][lexer][operators]") {
    auto ast = parse_test("fn main() { x -> y; }");
    (void)ast;
}

TEST_CASE("SCAN: not operator tokenized", "[scan][lexer][operators]") {
    auto ast = parse_test("fn main() { not x; }");
    (void)ast;
}

// ============================================================================
// Complex combinations
// ============================================================================

TEST_CASE("SCAN: complex program with multiple keywords", "[scan][complex]") {
    auto ast = parse_test("import std.io;\n"
                          "export std.io.println;\n"
                          "\n"
                          "trait Printable {\n"
                          "    fn print() -> void;\n"
                          "}\n"
                          "\n"
                          "struct Point {\n"
                          "    x: i32,\n"
                          "    y: i32,\n"
                          "    fn distance() -> f64 { }\n"
                          "}\n"
                          "\n"
                          "implement Printable for Point {\n"
                          "    fn print() -> void { }\n"
                          "}\n"
                          "\n"
                          "async fn fetch_data() -> i32 { }\n"
                          "\n"
                          "noreturn fn panic(msg) { }\n"
                          "\n"
                          "public fn main() { }\n");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len >= 5);
}

TEST_CASE("SCAN: struct with ownership modifiers", "[scan][ownership]") {
    auto ast = parse_test("struct Node {\n"
                          "    child: unique Node,\n"
                          "    parent: share Node,\n"
                          "}");
    REQUIRE(ast);

    auto *sdecl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    auto *sp    = static_cast<ZithStructPayload *>(sdecl->data.list.ptr);
    REQUIRE(sp->field_count == 2);
}

TEST_CASE("SCAN: struct with destructure fields [x,y]: i32", "[scan][struct][destructure]") {
    auto ast = parse_test("struct Point {\n"
                          "    [x, y]: i32,\n"
                          "}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_STRUCT_DECL);

    auto *sp = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    REQUIRE(sp->field_count == 2);

    auto *f0 = static_cast<ZithFieldPayload *>(sp->fields[0]->data.list.ptr);
    REQUIRE(std::string(f0->name, f0->name_len) == "x");
    REQUIRE(f0->type_node != nullptr);

    auto *f1 = static_cast<ZithFieldPayload *>(sp->fields[1]->data.list.ptr);
    REQUIRE(std::string(f1->name, f1->name_len) == "y");
    REQUIRE(f1->type_node != nullptr);
}

TEST_CASE("SCAN: struct with destructure tuple type [x,y]: |i32,u64|", "[scan][struct][destructure]") {
    auto ast = parse_test("struct Pair {\n"
                          "    [x, y]: |i32, u64|,\n"
                          "}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_STRUCT_DECL);

    auto *sp = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    REQUIRE(sp->field_count == 2);

    auto *f0 = static_cast<ZithFieldPayload *>(sp->fields[0]->data.list.ptr);
    REQUIRE(std::string(f0->name, f0->name_len) == "x");
    REQUIRE(f0->type_node != nullptr);
    REQUIRE(f0->type_node->type == ZITH_NODE_IDENTIFIER);

    auto *f1 = static_cast<ZithFieldPayload *>(sp->fields[1]->data.list.ptr);
    REQUIRE(std::string(f1->name, f1->name_len) == "y");
    REQUIRE(f1->type_node != nullptr);
    REQUIRE(f1->type_node->type == ZITH_NODE_IDENTIFIER);
}

TEST_CASE("SCAN: struct with let destructure fields let [a,b]: i32", "[scan][struct][destructure]") {
    auto ast = parse_test("struct Data {\n"
                          "    let [a, b]: i32,\n"
                          "}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    auto *sp = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    REQUIRE(sp->field_count == 2);
}

TEST_CASE("SCAN: function with error union return", "[scan][errors]") {
    auto ast = parse_test("fn operation() -> i32! { }");
    REQUIRE(ast);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    auto *p    = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(p->return_type != nullptr);
}

// ============================================================================
// Error handling - Generic
// ============================================================================

TEST_CASE("SCAN: null source returns nullptr", "[scan][error]") {
    auto *node = zith_parse_test(nullptr);
    REQUIRE(node == nullptr);
}

TEST_CASE("SCAN: invalid token produces error diagnostic", "[scan][error]") {
    auto *node = zith_parse_test("@@@");
    (void)node; // diagnostics go to stderr
}

// ============================================================================
// RAII / Arena behavior
// ============================================================================

TEST_CASE("ParseResult: move semantics", "[raii]") {
    auto r1 = parse_test("fn foo() { }");
    REQUIRE(r1);
    auto *ptr = r1.get();

    auto r2 = std::move(r1);
    REQUIRE(r1.get() == nullptr);
    REQUIRE(r2.get() == ptr);
}

TEST_CASE("ParseResult: reset invalidates node", "[raii]") {
    auto r = parse_test("fn foo() { }");
    REQUIRE(r);
    r.reset();
    REQUIRE(r.get() == nullptr);
}

TEST_CASE("ParseResult: double reset is safe", "[raii]") {
    auto r = parse_test("fn foo() { }");
    r.reset();
    r.reset();
    REQUIRE(r.get() == nullptr);
}

TEST_CASE("Arena reuse: second parse resets arena", "[raii]") {
    auto r1 = parse_test("fn foo() { }");
    REQUIRE(r1);

    auto r2 = parse_test("fn bar() { }");
    REQUIRE(r2);

    auto **decls = static_cast<ZithNode **>(r2->data.list.ptr);
    auto *p      = static_cast<ZithFuncPayload *>(decls[0]->data.list.ptr);
    REQUIRE(p->name[0] == 'b');
}

// ============================================================================
// comptime, global, const on struct fields
// ============================================================================

TEST_CASE("SCAN: comptime variable declaration", "[scan][comptime]") {
    auto ast = parse_test("comptime x: i32 = 42;");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_VAR_DECL);

    auto *p = static_cast<ZithVarPayload *>(decl->data.list.ptr);
    REQUIRE(std::string(p->name, p->name_len) == "x");
    REQUIRE(p->binding == ZITH_BINDING_COMPTIME);
}

TEST_CASE("SCAN: comptime fn declaration", "[scan][comptime]") {
    auto ast = parse_test("comptime fn foo(): i32 { return 42; }");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);

    auto *p = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(std::string(p->name, p->name_len) == "foo");
    REQUIRE(p->kind == ZITH_FN_COMPTIME);
}

TEST_CASE("SCAN: struct with global field", "[scan][struct]") {
    auto ast = parse_test("struct Config {\n"
                          "    global name: string,\n"
                          "}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_STRUCT_DECL);
    auto *sp = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    REQUIRE(sp->field_count == 1);
}

TEST_CASE("SCAN: struct with const field", "[scan][struct]") {
    auto ast = parse_test("struct Constants {\n"
                          "    const pi: f64,\n"
                          "}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_STRUCT_DECL);
    auto *sp = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    REQUIRE(sp->field_count == 1);
}

TEST_CASE("SCAN: struct with comptime field", "[scan][struct]") {
    auto ast = parse_test("struct Meta {\n"
                          "    comptime value: i32,\n"
                          "}");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_STRUCT_DECL);
    auto *sp = static_cast<ZithStructPayload *>(decl->data.list.ptr);
    REQUIRE(sp->field_count == 1);
}

TEST_CASE("SCAN: comptime used inside function body", "[scan][comptime]") {
    auto ast = parse_test("fn main() {\n"
                          "    comptime x: i32 = 10;\n"
                          "}");
    REQUIRE(ast);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);
}

// ============================================================================
// Destructure & Pack Literals
// ============================================================================

TEST_CASE("SCAN: destructure declaration let [x,y,z]: i32", "[scan][destructure]") {
    auto ast = parse_test("fn main() { let [x,y,z]: i32; }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: destructure with pack init let [x,y,z] = |1,0,-1|", "[scan][destructure]") {
    auto ast = parse_test("fn main() { let [x,y,z] = |1,0,-1|; }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: destructure with scalar init let [x,y,z] = 5", "[scan][destructure]") {
    auto ast = parse_test("fn main() { let [x,y,z] = 5; }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: pack literal |1,2,3|", "[scan][pack]") {
    auto ast = parse_test("fn main() { let p = |1,2,3|; }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: pack literal empty pipe", "[scan][pack]") {
    auto ast = parse_test("fn main() { let empty = ||; }");
    REQUIRE(ast);
}

// ============================================================================
// Function Prototypes (body-less fn declarations)
// ============================================================================

TEST_CASE("SCAN: function prototype fn add();", "[scan][prototype]") {
    auto ast = parse_test("fn add();");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);

    auto *p = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(std::string(p->name, p->name_len) == "add");
    REQUIRE(p->param_count == 0);
    REQUIRE(p->body == nullptr);
    REQUIRE(p->return_type == nullptr);
}

TEST_CASE("SCAN: function prototype with params and return fn add(a: i32): bool;", "[scan][prototype]") {
    auto ast = parse_test("fn add(a: i32): bool;");
    REQUIRE(ast);
    REQUIRE(ast->data.list.len == 1);

    auto *decl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(decl->type == ZITH_NODE_FUNC_DECL);

    auto *p = static_cast<ZithFuncPayload *>(decl->data.list.ptr);
    REQUIRE(std::string(p->name, p->name_len) == "add");
    REQUIRE(p->param_count == 1);
    REQUIRE(p->body == nullptr);
    REQUIRE(p->return_type != nullptr);
}

// ============================================================================
// Struct destructure error cases
// ============================================================================

TEST_CASE("SCAN: struct destructure type count mismatch (more types than names)", "[scan][struct][destructure][error]") {
    auto ast = parse_test("struct Bad {\n"
                          "    [x, y]: |i32, f32, f64|,\n"
                          "}");
    REQUIRE(ast);

    auto *sdecl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(sdecl->type == ZITH_NODE_STRUCT_DECL);

    auto *sp = static_cast<ZithStructPayload *>(sdecl->data.list.ptr);
    REQUIRE(sp->field_count == 2);
    for (size_t i = 0; i < sp->field_count; ++i) {
        auto *f = static_cast<ZithFieldPayload *>(sp->fields[i]->data.list.ptr);
        REQUIRE(f->type_node != nullptr);
    }
}

TEST_CASE("SCAN: struct destructure type count mismatch (more names than types)", "[scan][struct][destructure][error]") {
    auto ast = parse_test("struct Bad {\n"
                          "    [x, y, z]: |i32, f32|,\n"
                          "}");
    REQUIRE(ast);

    auto *sdecl = static_cast<ZithNode **>(ast->data.list.ptr)[0];
    REQUIRE(sdecl->type == ZITH_NODE_STRUCT_DECL);

    auto *sp = static_cast<ZithStructPayload *>(sdecl->data.list.ptr);
    REQUIRE(sp->field_count == 3);
}

// ============================================================================
// Call-site ownership annotations
// ============================================================================

TEST_CASE("SCAN: call with view ownership annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(view x, view y); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: call with lend ownership annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(lend x); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: call with extension ownership annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(extension x); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: call with unique ownership annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(unique x); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: call with share ownership annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(share x); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: call without ownership annotation (default)", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(x, y); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: method call with view ownership annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { obj.method(view x); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: call with mixed annotations", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(view a, b, lend c, d); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: nested call with annotations", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { foo(view bar(lend x)); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: arrow call with view annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { x->foo(view y); }");
    REQUIRE(ast);
}

TEST_CASE("SCAN: literal argument with view annotation", "[scan][call][ownership]") {
    auto ast = parse_test("fn test() { add(view 42); }");
    REQUIRE(ast);
}

TEST_CASE("FULL: call with view ownership annotation", "[full][call][ownership]") {
    auto ast = zith_parse_test_full("fn test(a: view i32) { }\n"
                                    "fn main() { let x: i32 = 42; test(view x); }");
    REQUIRE(ast);
}

TEST_CASE("FULL: call with lend ownership annotation", "[full][call][ownership]") {
    auto ast = zith_parse_test_full("fn test(a: lend i32) { }\n"
                                    "fn main() { let x: i32 = 42; test(lend x); }");
    REQUIRE(ast);
}

TEST_CASE("FULL: call with extension ownership annotation", "[full][call][ownership]") {
    auto ast = zith_parse_test_full("fn test(a: extension i32) { }\n"
                                    "fn main() { let x: i32 = 42; test(extension x); }");
    REQUIRE(ast);
}

TEST_CASE("FULL: call_arg AST ownership is correct for view", "[full][call][ownership][ast]") {
    auto ast = zith_parse_test_full("fn id(x: view i32): i32 { return x; }\n"
                                    "fn main() { let x: i32 = 42; let y = id(view x); }");
    REQUIRE(ast);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len == 2);

    auto *main_fn   = static_cast<ZithFuncPayload *>(decls[1]->data.list.ptr);
    auto *block     = main_fn->body;
    auto **stmts    = static_cast<ZithNode **>(block->data.list.ptr);
    REQUIRE(block->data.list.len >= 2);

    auto *var_decl  = static_cast<ZithVarPayload *>(stmts[1]->data.list.ptr);
    auto *call      = static_cast<ZithCallPayload *>(var_decl->initializer->data.list.ptr);
    REQUIRE(call->arg_count == 1);
    REQUIRE(call->args[0]->type == ZITH_NODE_CALL_ARG);

    auto *arg = static_cast<ZithCallArgPayload *>(call->args[0]->data.list.ptr);
    REQUIRE(arg->ownership == ZITH_OWN_VIEW);
    REQUIRE(arg->expr->type == ZITH_NODE_IDENTIFIER);
}

TEST_CASE("FULL: call_arg AST ownership is correct for lend", "[full][call][ownership][ast]") {
    auto ast = zith_parse_test_full("fn id(x: lend i32): i32 { return x; }\n"
                                    "fn main() { let x: i32 = 42; let y = id(lend x); }");
    REQUIRE(ast);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len == 2);

    auto *main_fn   = static_cast<ZithFuncPayload *>(decls[1]->data.list.ptr);
    auto *block     = main_fn->body;
    auto **stmts    = static_cast<ZithNode **>(block->data.list.ptr);
    REQUIRE(block->data.list.len >= 2);

    auto *var_decl  = static_cast<ZithVarPayload *>(stmts[1]->data.list.ptr);
    auto *call      = static_cast<ZithCallPayload *>(var_decl->initializer->data.list.ptr);
    REQUIRE(call->arg_count == 1);
    REQUIRE(call->args[0]->type == ZITH_NODE_CALL_ARG);

    auto *arg = static_cast<ZithCallArgPayload *>(call->args[0]->data.list.ptr);
    REQUIRE(arg->ownership == ZITH_OWN_LEND);
    REQUIRE(arg->expr->type == ZITH_NODE_IDENTIFIER);
}

TEST_CASE("FULL: call_arg AST ownership is correct for extension", "[full][call][ownership][ast]") {
    auto ast = zith_parse_test_full("fn id(x: extension i32): i32 { return x; }\n"
                                    "fn main() { let x: i32 = 42; let y = id(extension x); }");
    REQUIRE(ast);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len == 2);

    auto *main_fn   = static_cast<ZithFuncPayload *>(decls[1]->data.list.ptr);
    auto *block     = main_fn->body;
    auto **stmts    = static_cast<ZithNode **>(block->data.list.ptr);
    REQUIRE(block->data.list.len >= 2);

    auto *var_decl  = static_cast<ZithVarPayload *>(stmts[1]->data.list.ptr);
    auto *call      = static_cast<ZithCallPayload *>(var_decl->initializer->data.list.ptr);
    REQUIRE(call->arg_count == 1);
    REQUIRE(call->args[0]->type == ZITH_NODE_CALL_ARG);

    auto *arg = static_cast<ZithCallArgPayload *>(call->args[0]->data.list.ptr);
    REQUIRE(arg->ownership == ZITH_OWN_EXTENSION);
}

TEST_CASE("FULL: call_arg AST ownership is correct for share", "[full][call][ownership][ast]") {
    auto ast = zith_parse_test_full("fn id(x: share i32): i32 { return x; }\n"
                                    "fn main() { let x: i32 = 42; let y = id(share x); }");
    REQUIRE(ast);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len == 2);

    auto *main_fn   = static_cast<ZithFuncPayload *>(decls[1]->data.list.ptr);
    auto *block     = main_fn->body;
    auto **stmts    = static_cast<ZithNode **>(block->data.list.ptr);
    REQUIRE(block->data.list.len >= 2);

    auto *var_decl  = static_cast<ZithVarPayload *>(stmts[1]->data.list.ptr);
    auto *call      = static_cast<ZithCallPayload *>(var_decl->initializer->data.list.ptr);
    REQUIRE(call->arg_count == 1);
    REQUIRE(call->args[0]->type == ZITH_NODE_CALL_ARG);

    auto *arg = static_cast<ZithCallArgPayload *>(call->args[0]->data.list.ptr);
    REQUIRE(arg->ownership == ZITH_OWN_SHARED);
}

TEST_CASE("FULL: call_arg AST ownership is correct for unique", "[full][call][ownership][ast]") {
    auto ast = zith_parse_test_full("fn id(x: unique i32): i32 { return x; }\n"
                                    "fn main() { let x: i32 = 42; let y = id(unique x); }");
    REQUIRE(ast);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len == 2);

    auto *main_fn   = static_cast<ZithFuncPayload *>(decls[1]->data.list.ptr);
    auto *block     = main_fn->body;
    auto **stmts    = static_cast<ZithNode **>(block->data.list.ptr);
    REQUIRE(block->data.list.len >= 2);

    auto *var_decl  = static_cast<ZithVarPayload *>(stmts[1]->data.list.ptr);
    auto *call      = static_cast<ZithCallPayload *>(var_decl->initializer->data.list.ptr);
    REQUIRE(call->arg_count == 1);
    REQUIRE(call->args[0]->type == ZITH_NODE_CALL_ARG);

    auto *arg = static_cast<ZithCallArgPayload *>(call->args[0]->data.list.ptr);
    REQUIRE(arg->ownership == ZITH_OWN_UNIQUE);
}

TEST_CASE("FULL: default arg has no CALL_ARG wrapper", "[full][call][ownership][ast]") {
    auto ast = zith_parse_test_full("fn id(x: i32): i32 { return x; }\n"
                                    "fn main() { let x: i32 = 42; let y = id(x); }");
    REQUIRE(ast);

    auto **decls = static_cast<ZithNode **>(ast->data.list.ptr);
    REQUIRE(ast->data.list.len == 2);

    auto *main_fn   = static_cast<ZithFuncPayload *>(decls[1]->data.list.ptr);
    auto *block     = main_fn->body;
    auto **stmts    = static_cast<ZithNode **>(block->data.list.ptr);
    REQUIRE(block->data.list.len >= 2);

    auto *var_decl  = static_cast<ZithVarPayload *>(stmts[1]->data.list.ptr);
    auto *call      = static_cast<ZithCallPayload *>(var_decl->initializer->data.list.ptr);
    REQUIRE(call->arg_count == 1);
    REQUIRE(call->args[0]->type == ZITH_NODE_IDENTIFIER);
}

// ============================================================================
// Cleanup
// ============================================================================
// Cleanup
// ============================================================================

TEST_CASE("Arena destroy at end", "[cleanup]") {
    zith_test_arena_destroy();
}
