#include "../test-common.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "import/symbol-table.hpp"
#include "import/symbol-visibility.hpp"
#include "lexer/lexer.hpp"
#include "memory/arena.hpp"
#include "memory/source-map.hpp"
#include "parser/parser.hpp"
static auto source_map = zith::memory::SourceMap();

using namespace zith::lexer;
using namespace zith::ast;
using namespace zith::import;
using zith::diagnostics::DiagnosticEngine;
using zith::memory::Arena;

static Arena make_arena() {
    return Arena();
}

// ── ImportNode from "from" syntax ────────────────────────────

static void test_parse_from_import() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "from std/io/console", diags);
    CHECK(token_result.isOk(), "tokenize 'from std/io/console'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'from std/io/console'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "import path has 3 segments");
    CHECK(import.path[0] == "std", "first segment is 'std'");
    CHECK(import.path[1] == "io", "second segment is 'io'");
    CHECK(import.path[2] == "console", "third segment is 'console'");
}

// ── ImportNode from "import" syntax ──────────────────────────

static void test_parse_import_single() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import foo", diags);
    CHECK(token_result.isOk(), "tokenize 'import foo'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import foo'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(1), "import path has 1 segment");
    CHECK(import.path[0] == "foo", "first segment is 'foo'");
}

// ── SymbolTable visibility helpers ───────────────────────────

static void test_syms_declare_visibility() {
    Arena arena = make_arena();
    SymbolTable syms(arena);

    auto id_pub  = syms.declare("pub_fn", SymbolVisibility::Public, 0);
    auto id_priv = syms.declare("priv_fn", SymbolVisibility::Private, 0);
    auto id_mod  = syms.declare("mod_fn", SymbolVisibility::Module, 3);

    CHECK_EQ(syms.get(id_pub).visibility, SymbolVisibility::Public, "pub symbol visibility");
    CHECK_EQ(syms.get(id_priv).visibility, SymbolVisibility::Private, "priv symbol visibility");
    CHECK_EQ(syms.get(id_mod).visibility, SymbolVisibility::Module, "mod symbol visibility");
    CHECK_EQ(syms.get(id_mod).mod_depth, int32_t(3), "mod depth is 3");
}

// ── pub visibility via scanner ────────────────────────────────

static void test_scanner_pub_visibility() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto token_result = tokenize(source_map, arena, "test", "pub fn foo() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'pub fn foo() {}'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(1), "1 symbol registered");

    auto &data = syms.get(0);
    CHECK(data.name == "foo", "symbol name is 'foo'");
    CHECK_EQ(data.visibility, SymbolVisibility::Public, "visibility is Public");
}

// ── mod default depth via scanner ────────────────────────────

static void test_scanner_mod_default() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto token_result = tokenize(source_map, arena, "test", "mod fn bar() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'mod fn bar() {}'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(1), "1 symbol registered");

    auto &data = syms.get(0);
    CHECK(data.name == "bar", "symbol name is 'bar'");
    CHECK_EQ(data.visibility, SymbolVisibility::Module, "visibility is Module");
    CHECK_EQ(data.mod_depth, int32_t(1), "mod_depth defaults to 1");
}

// ── mod(N) depth via scanner ─────────────────────────────────

static void test_scanner_mod_n_depth() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto token_result = tokenize(source_map, arena, "test", "mod(3) fn baz() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'mod(3) fn baz() {}'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(1), "1 symbol registered");

    auto &data = syms.get(0);
    CHECK(data.name == "baz", "symbol name is 'baz'");
    CHECK_EQ(data.visibility, SymbolVisibility::Module, "visibility is Module");
    CHECK_EQ(data.mod_depth, int32_t(3), "mod_depth is 3");
}

// ── mod(..) infinite depth via scanner ────────────────────────

static void test_scanner_mod_infinite_depth() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto token_result = tokenize(source_map, arena, "test", "mod(..) fn qux() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'mod(..) fn qux() {}'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(1), "1 symbol registered");

    auto &data = syms.get(0);
    CHECK(data.name == "qux", "symbol name is 'qux'");
    CHECK_EQ(data.visibility, SymbolVisibility::Module, "visibility is Module");
    CHECK_EQ(data.mod_depth, int32_t(-1), "mod_depth is -1 (infinite)");
}

// ── private is default visibility ────────────────────────────

static void test_scanner_private_default() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto token_result = tokenize(source_map, arena, "test", "fn plain() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'fn plain() {}'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(1), "1 symbol registered");

    auto &data = syms.get(0);
    CHECK(data.name == "plain", "symbol name is 'plain'");
    CHECK_EQ(data.visibility, SymbolVisibility::Private, "visibility defaults to Private");
    CHECK_EQ(data.mod_depth, int32_t(0), "mod_depth is 0 for private symbols");
}

// ── visibility resets after each decl ────────────────────────

static void test_scanner_visibility_resets() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto token_result =
        tokenize(source_map, arena, "test", "pub fn first() {}\nfn second() {}", diags);
    CHECK(token_result.isOk(), "tokenize two fns with pub on first");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(2), "2 symbols registered");

    auto &first = syms.get(0);
    CHECK(first.name == "first", "first symbol is 'first'");
    CHECK_EQ(first.visibility, SymbolVisibility::Public, "first is Public");

    auto &second = syms.get(1);
    CHECK(second.name == "second", "second symbol is 'second'");
    CHECK_EQ(second.visibility, SymbolVisibility::Private, "second is Private (reset)");
}

// ── struct with pub visibility ───────────────────────────────

static void test_scanner_pub_struct() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);
    SymbolTable syms(arena);

    auto token_result = tokenize(source_map, arena, "test", "pub struct Point", diags);
    CHECK(token_result.isOk(), "tokenize 'pub struct Point'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(1), "1 symbol registered");
    auto &data = syms.get(0);
    CHECK(data.name == "Point", "symbol name is 'Point'");
    CHECK_EQ(data.visibility, SymbolVisibility::Public, "struct visibility is Public");
}

// ── export keyword ──────────────────────────────────────────────

static void test_parse_export() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "export std/io/console", diags);
    CHECK(token_result.isOk(), "tokenize 'export std/io/console'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'export std/io/console'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "export path has 3 segments");
    CHECK(import.path[0] == "std", "first segment is 'std'");
    CHECK(import.path[1] == "io", "second segment is 'io'");
    CHECK(import.path[2] == "console", "third segment is 'console'");
    CHECK(import.is_export, "is_export is true");
}

// ── Multi-segment import path ───────────────────────────────────

static void test_import_multi_segment() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import foo/bar/baz", diags);
    CHECK(token_result.isOk(), "tokenize 'import foo/bar/baz'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import foo/bar/baz'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "import path has 3 segments");
    CHECK(import.path[0] == "foo", "first segment is 'foo'");
    CHECK(import.path[1] == "bar", "second segment is 'bar'");
    CHECK(import.path[2] == "baz", "third segment is 'baz'");
    CHECK(!import.is_from, "is_from is false");
    CHECK(!import.is_export, "is_export is false");
}

// ── import path as alias ────────────────────────────────────────

static void test_import_alias() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import std/io/console as con", diags);
    CHECK(token_result.isOk(), "tokenize 'import std/io/console as con'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import std/io/console as con'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "import path has 3 segments");
    CHECK(import.path[0] == "std", "first segment is 'std'");
    CHECK(import.path[1] == "io", "second segment is 'io'");
    CHECK(import.path[2] == "console", "third segment is 'console'");
    CHECK(import.alias == "con", "alias is 'con'");
    CHECK(!import.is_from, "is_from is false");
}

// ── .. in path segments ─────────────────────────────────────────

static void test_import_dotdot() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "from ../lib/mymod", diags);
    CHECK(token_result.isOk(), "tokenize 'from ../lib/mymod'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'from ../lib/mymod'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "import path has 3 segments");
    CHECK(import.path[0] == "..", "first segment is '..'");
    CHECK(import.path[1] == "lib", "second segment is 'lib'");
    CHECK(import.path[2] == "mymod", "third segment is 'mymod'");
    CHECK(import.is_from, "is_from is true");
}

// ── export with multi-segment path ───────────────────────────────

static void test_export_multi() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "export foo/bar/baz", diags);
    CHECK(token_result.isOk(), "tokenize 'export foo/bar/baz'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'export foo/bar/baz'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "export path has 3 segments");
    CHECK(import.path[0] == "foo", "first segment is 'foo'");
    CHECK(import.path[1] == "bar", "second segment is 'bar'");
    CHECK(import.path[2] == "baz", "third segment is 'baz'");
    CHECK(import.is_export, "is_export is true");
    CHECK(import.is_from, "export uses is_from=true");
}

// ── export with .. ───────────────────────────────────────────────

static void test_export_dotdot() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "export ../lib/mymod", diags);
    CHECK(token_result.isOk(), "tokenize 'export ../lib/mymod'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'export ../lib/mymod'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "export path has 3 segments");
    CHECK(import.path[0] == "..", "first segment is '..'");
    CHECK(import.path[1] == "lib", "second segment is 'lib'");
    CHECK(import.path[2] == "mymod", "third segment is 'mymod'");
    CHECK(import.is_export, "is_export is true");
    CHECK(import.is_from, "export uses is_from=true");
}

// ── import with depth (N) ─────────────────────────────────────────

static void test_import_depth_n() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import mymod(3)", diags);
    CHECK(token_result.isOk(), "tokenize 'import mymod(3)'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import mymod(3)'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(1), "import path has 1 segment");
    CHECK(import.path[0] == "mymod", "first segment is 'mymod'");
    CHECK_EQ(import.import_depth, int32_t(3), "import_depth is 3");
}

// ── import with depth (..) ────────────────────────────────────────

static void test_import_depth_infinite() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import mymod(..)", diags);
    CHECK(token_result.isOk(), "tokenize 'import mymod(..)'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import mymod(..)'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(1), "import path has 1 segment");
    CHECK(import.path[0] == "mymod", "first segment is 'mymod'");
    CHECK_EQ(import.import_depth, int32_t(-1), "import_depth is -1 (infinite)");
}

// ── from with depth (N) ───────────────────────────────────────────

static void test_from_depth_n() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "from mymod(2)", diags);
    CHECK(token_result.isOk(), "tokenize 'from mymod(2)'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'from mymod(2)'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(1), "import path has 1 segment");
    CHECK(import.path[0] == "mymod", "first segment is 'mymod'");
    CHECK_EQ(import.import_depth, int32_t(2), "import_depth is 2");
    CHECK(import.is_from, "is_from is true");
}

// ── import with depth + alias ─────────────────────────────────────

static void test_import_depth_alias() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import mymod(3) as m", diags);
    CHECK(token_result.isOk(), "tokenize 'import mymod(3) as m'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import mymod(3) as m'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(1), "import path has 1 segment");
    CHECK(import.path[0] == "mymod", "first segment is 'mymod'");
    CHECK_EQ(import.import_depth, int32_t(3), "import_depth is 3");
    CHECK(import.alias == "m", "alias is 'm'");
    CHECK(!import.is_from, "is_from is false");
}

// ── import with multi-segment path + depth + alias ────────────────

static void test_import_multi_depth_alias() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import foo/bar(5) as b", diags);
    CHECK(token_result.isOk(), "tokenize 'import foo/bar(5) as b'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import foo/bar(5) as b'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(2), "import path has 2 segments");
    CHECK(import.path[0] == "foo", "first segment is 'foo'");
    CHECK(import.path[1] == "bar", "second segment is 'bar'");
    CHECK_EQ(import.import_depth, int32_t(5), "import_depth is 5");
    CHECK(import.alias == "b", "alias is 'b'");
}

// ── import alias with multi-segment ──────────────────────────────

static void test_import_alias_multi() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import foo/bar/baz as x", diags);
    CHECK(token_result.isOk(), "tokenize 'import foo/bar/baz as x'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import foo/bar/baz as x'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "import path has 3 segments");
    CHECK(import.path[0] == "foo", "first segment is 'foo'");
    CHECK(import.path[1] == "bar", "second segment is 'bar'");
    CHECK(import.path[2] == "baz", "third segment is 'baz'");
    CHECK(import.alias == "x", "alias is 'x'");
    CHECK(!import.is_from, "is_from is false");
}

// ── import with .. as alias ──────────────────────────────────────

static void test_import_dotdot_as_alias() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import ../lib/mymod as m", diags);
    CHECK(token_result.isOk(), "tokenize 'import ../lib/mymod as m'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import ../lib/mymod as m'");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl))
        return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "import path has 3 segments");
    CHECK(import.path[0] == "..", "first segment is '..'");
    CHECK(import.path[1] == "lib", "second segment is 'lib'");
    CHECK(import.path[2] == "mymod", "third segment is 'mymod'");
    CHECK(import.alias == "m", "alias is 'm'");
    CHECK(!import.is_from, "is_from is false");
}

// ── empty import path (should not crash, no import created) ──────

static void test_empty_import() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "import", diags);
    CHECK(token_result.isOk(), "tokenize 'import'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import' (no crash)");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(0), "no decls for empty import");
}

// ── empty from path (should not crash, no import created) ────────

static void test_empty_from() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "from", diags);
    CHECK(token_result.isOk(), "tokenize 'from'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'from' (no crash)");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(0), "no decls for empty from");
}

// ── empty export path (should not crash, no import created) ──────

static void test_empty_export() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags(arena);

    auto token_result = tokenize(source_map, arena, "test", "export", diags);
    CHECK(token_result.isOk(), "tokenize 'export'");
    if (!token_result.isOk())
        return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'export' (no crash)");
    if (!result.isOk())
        return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(0), "no decls for empty export");
}

int main() {
    std::printf("import-basics tests\n");
    std::printf("======================\n\n");

    test_parse_from_import();
    test_parse_import_single();
    test_parse_export();
    test_export_multi();
    test_export_dotdot();
    test_import_multi_segment();
    test_import_alias();
    test_import_alias_multi();
    test_import_depth_n();
    test_import_depth_infinite();
    test_from_depth_n();
    test_import_depth_alias();
    test_import_multi_depth_alias();
    test_import_dotdot();
    test_import_dotdot_as_alias();
    test_empty_import();
    test_empty_from();
    test_empty_export();
    test_syms_declare_visibility();
    test_scanner_pub_visibility();
    test_scanner_mod_default();
    test_scanner_mod_n_depth();
    test_scanner_mod_infinite_depth();
    test_scanner_private_default();
    test_scanner_visibility_resets();
    test_scanner_pub_struct();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
