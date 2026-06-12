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

auto g_source_map = zith::memory::SourceMap();

using namespace zith::lexer;
using namespace zith::ast;
using zith::diagnostics::DiagnosticEngine;
using zith::import::SymbolTable;
using zith::memory::Arena;
using zith::parser::scan;
using zith::parser::ScanResult;
using zith::parser::Parser;

// helper: run scan only (no expand) on src, return ScanResult + SymbolTable
static void do_scan(Arena &arena, AstBuilder &builder, DiagnosticEngine &diags, SymbolTable &syms,
                    std::string_view src, ScanResult &out_result) {
    auto toks = tokenize(g_source_map, arena, "test", std::string(src), diags).value();
    Parser parser(&toks, &builder, &diags);
    auto result = scan(parser, syms);
    out_result = std::move(result);
}

// ── struct scan tests ─────────────────────────────────────────────────────

static void test_scan_empty_struct() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms, "struct Empty {}", result);

    CHECK_EQ(result.structs.size(), size_t(1), "struct scan result has 1 entry");
    CHECK(result.fns.empty(), "no function entries");
    CHECK(result.enums.empty(), "no enum entries");
    CHECK(result.unions.empty(), "no union entries");
    CHECK(result.components.empty(), "no component entries");
    CHECK(result.traits.empty(), "no trait entries");

    CHECK(result.structs[0].name == "Empty", "struct name is Empty");

    auto id = syms.lookup("Empty");
    CHECK(id != zith::import::kInvalidSym, "Empty is registered in sym table");
    CHECK(syms.get(id).kind == zith::import::SymKind::Struct, "Empty is kind Struct");
}

static void test_scan_struct_with_fields() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms,
            "struct Point { x: i32, y: f64, }", result);

    CHECK_EQ(result.structs.size(), size_t(1), "one struct scanned");

    auto x = syms.lookup("x");
    auto y = syms.lookup("y");
    CHECK(x != zith::import::kInvalidSym, "field x registered");
    CHECK(y != zith::import::kInvalidSym, "field y registered");
    CHECK_EQ(syms.get(x).kind, zith::import::SymKind::Variable, "field x is Variable kind");

    auto z = syms.lookup("z");
    CHECK(z == zith::import::kInvalidSym, "field z not registered (does not exist)");
}

static void test_scan_struct_with_methods() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms,
            "struct Foo { data: i32, fn get() -> i32 { return data; } fn set(v: i32) {} }",
            result);

    CHECK_EQ(result.structs.size(), size_t(1), "one struct scanned");
    CHECK(syms.lookup("data") != zith::import::kInvalidSym, "field data registered");
    CHECK(syms.lookup("get") != zith::import::kInvalidSym, "method get registered");
    CHECK(syms.lookup("set") != zith::import::kInvalidSym, "method set registered");
}

static void test_scan_enum_simple() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms, "enum Color { Red, Green, Blue }", result);

    CHECK_EQ(result.enums.size(), size_t(1), "one enum scanned");
    CHECK(syms.lookup("Red") != zith::import::kInvalidSym, "variant Red registered");
    CHECK(syms.lookup("Green") != zith::import::kInvalidSym, "variant Green registered");
    CHECK(syms.lookup("Blue") != zith::import::kInvalidSym, "variant Blue registered");
    CHECK(syms.lookup("Color") != zith::import::kInvalidSym, "enum Color registered");
    CHECK_EQ(syms.get(syms.lookup("Color")).kind, zith::import::SymKind::Enum, "Color is kind Enum");
}

static void test_scan_union_simple() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms, "union Value { i: i64, f: f64 }", result);

    CHECK_EQ(result.unions.size(), size_t(1), "one union scanned");
    CHECK(syms.lookup("i") != zith::import::kInvalidSym, "variant i registered");
    CHECK(syms.lookup("f") != zith::import::kInvalidSym, "variant f registered");
    CHECK(syms.lookup("Value") != zith::import::kInvalidSym, "union Value registered");
    CHECK_EQ(syms.get(syms.lookup("Value")).kind, zith::import::SymKind::Union, "Value is kind Union");
}

static void test_scan_component_empty() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms, "component Empty {}", result);

    CHECK_EQ(result.components.size(), size_t(1), "one component scanned");
    CHECK(syms.lookup("Empty") != zith::import::kInvalidSym, "Empty component registered");
    CHECK_EQ(syms.get(syms.lookup("Empty")).kind, zith::import::SymKind::Component,
             "Empty is kind Component");
}

static void test_scan_trait_simple() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms, "trait Iterator { fn next() -> i32; fn has_next() -> bool; }",
            result);

    CHECK_EQ(result.traits.size(), size_t(1), "one trait scanned");
    CHECK(syms.lookup("Iterator") != zith::import::kInvalidSym, "trait Iterator registered");
    CHECK(syms.lookup("next") != zith::import::kInvalidSym, "method next registered");
    CHECK(syms.lookup("has_next") != zith::import::kInvalidSym, "method has_next registered");
}

static void test_scan_interface() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms, "interface Stream { fn read() -> u8; fn write(b: u8); }",
            result);

    CHECK_EQ(result.traits.size(), size_t(1), "interface scanned as trait entry");
    CHECK(syms.lookup("Stream") != zith::import::kInvalidSym, "Stream registered");
    CHECK(syms.lookup("read") != zith::import::kInvalidSym, "method read registered");
    CHECK(syms.lookup("write") != zith::import::kInvalidSym, "method write registered");
}

static void test_scan_implement() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms,
            "implement Foo for Bar { fn method() {} }", result);

    // implement blocks produce no ScanEntry entries
    CHECK(result.fns.empty(), "no fn entries from implement");
    CHECK(result.structs.empty(), "no struct entries from implement");
    CHECK(!diags.hasErrors(), "no errors from implement");
}

static void test_scan_struct_public() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms, "pub struct PublicStruct { x: i32 }", result);

    CHECK_EQ(result.structs.size(), size_t(1), "one struct scanned");
    auto id = syms.lookup("PublicStruct");
    CHECK(id != zith::import::kInvalidSym, "PublicStruct registered");
    CHECK(syms.get(id).visibility == zith::import::SymbolVisibility::Public,
          "PublicStruct is public");
}

static void test_scan_duplicate_name() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms,
            "struct Foo {}\n"
            "enum Foo { A, B }",
            result);

    CHECK(diags.hasErrors(), "duplicate name across types reports error");
}

static void test_scan_multiple_decls() {
    Arena arena;
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);
    ScanResult result(arena);

    do_scan(arena, builder, diags, syms,
            "struct S { x: i32 }  enum E { A, B }  union U { a: i32 }  "
            "component C {}  trait T { fn foo(); }",
            result);

    CHECK_EQ(result.structs.size(), size_t(1), "1 struct");
    CHECK_EQ(result.enums.size(), size_t(1), "1 enum");
    CHECK_EQ(result.unions.size(), size_t(1), "1 union");
    CHECK_EQ(result.components.size(), size_t(1), "1 component");
    CHECK_EQ(result.traits.size(), size_t(1), "1 trait");
    CHECK(!diags.hasErrors(), "no errors with multiple distinct decls");
}

// ── main ──────────────────────────────────────────────────────────────────

int main() {
    std::printf("parser-scan tests\n");
    std::printf("====================\n\n");

    test_scan_empty_struct();
    test_scan_struct_with_fields();
    test_scan_struct_with_methods();
    test_scan_struct_public();

    test_scan_enum_simple();

    test_scan_union_simple();

    test_scan_component_empty();

    test_scan_trait_simple();
    test_scan_interface();
    test_scan_implement();

    test_scan_duplicate_name();
    test_scan_multiple_decls();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
