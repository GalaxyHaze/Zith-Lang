#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "import/symbol-table.hpp"
#include "import/symbol-visibility.hpp"
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
using namespace zith::import;
using zith::memory::Arena;
using zith::diagnostics::DiagnosticEngine;

static Arena make_arena() { return Arena(); }

// ── ImportNode from "from" syntax ────────────────────────────

static void test_parse_from_import() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags;

    auto token_result = tokenize("test", "from std/io/console", diags);
    CHECK(token_result.isOk(), "tokenize 'from std/io/console'");
    if (!token_result.isOk()) return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'from std/io/console'");
    if (!result.isOk()) return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl)) return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(3), "import path has 3 segments");
    CHECK(import.path[0] == "std", "first segment is 'std'");
    CHECK(import.path[1] == "io",  "second segment is 'io'");
    CHECK(import.path[2] == "console", "third segment is 'console'");
}

// ── ImportNode from "import" syntax ──────────────────────────

static void test_parse_import_single() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags;

    auto token_result = tokenize("test", "import foo", diags);
    CHECK(token_result.isOk(), "tokenize 'import foo'");
    if (!token_result.isOk()) return;

    auto tokens = std::move(token_result.value());
    auto result = zith::parser::parseProgram(tokens, builder, diags);
    CHECK(result.isOk(), "parse 'import foo'");
    if (!result.isOk()) return;

    auto &program = result.value();
    CHECK_EQ(program.decls.size(), size_t(1), "program has 1 decl");

    auto &decl = builder.getDecl(program.decls[0]);
    CHECK(std::holds_alternative<ImportNode>(decl), "decl is ImportNode");
    if (!std::holds_alternative<ImportNode>(decl)) return;

    auto &import = std::get<ImportNode>(decl);
    CHECK_EQ(import.path.size(), size_t(1), "import path has 1 segment");
    CHECK(import.path[0] == "foo", "first segment is 'foo'");
}

// ── SymbolTable visibility helpers ───────────────────────────

static void test_syms_declare_visibility() {
    Arena arena = make_arena();
    SymbolTable syms(arena);

    auto id_pub  = syms.declare("pub_fn",  SymbolVisibility::Public, 0);
    auto id_priv = syms.declare("priv_fn", SymbolVisibility::Private, 0);
    auto id_mod  = syms.declare("mod_fn",  SymbolVisibility::Module, 3);

    CHECK_EQ(syms.get(id_pub).visibility,  SymbolVisibility::Public,  "pub symbol visibility");
    CHECK_EQ(syms.get(id_priv).visibility, SymbolVisibility::Private, "priv symbol visibility");
    CHECK_EQ(syms.get(id_mod).visibility,  SymbolVisibility::Module,  "mod symbol visibility");
    CHECK_EQ(syms.get(id_mod).mod_depth,   int32_t(3),               "mod depth is 3");
}

// ── pub visibility via scanner ────────────────────────────────

static void test_scanner_pub_visibility() {
    Arena arena = make_arena();
    AstBuilder builder(arena);
    DiagnosticEngine diags;
    SymbolTable syms(arena);

    auto token_result = tokenize("test", "pub fn foo() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'pub fn foo() {}'");
    if (!token_result.isOk()) return;

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
    DiagnosticEngine diags;
    SymbolTable syms(arena);

    auto token_result = tokenize("test", "mod fn bar() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'mod fn bar() {}'");
    if (!token_result.isOk()) return;

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
    DiagnosticEngine diags;
    SymbolTable syms(arena);

    auto token_result = tokenize("test", "mod(3) fn baz() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'mod(3) fn baz() {}'");
    if (!token_result.isOk()) return;

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
    DiagnosticEngine diags;
    SymbolTable syms(arena);

    auto token_result = tokenize("test", "mod(..) fn qux() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'mod(..) fn qux() {}'");
    if (!token_result.isOk()) return;

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
    DiagnosticEngine diags;
    SymbolTable syms(arena);

    auto token_result = tokenize("test", "fn plain() {}", diags);
    CHECK(token_result.isOk(), "tokenize 'fn plain() {}'");
    if (!token_result.isOk()) return;

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
    DiagnosticEngine diags;
    SymbolTable syms(arena);

    auto token_result = tokenize("test", "pub fn first() {}\nfn second() {}", diags);
    CHECK(token_result.isOk(), "tokenize two fns with pub on first");
    if (!token_result.isOk()) return;

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
    DiagnosticEngine diags;
    SymbolTable syms(arena);

    auto token_result = tokenize("test", "pub struct Point", diags);
    CHECK(token_result.isOk(), "tokenize 'pub struct Point'");
    if (!token_result.isOk()) return;

    auto tokens = std::move(token_result.value());
    zith::parser::Parser parser{&tokens, &builder, &diags};
    zith::parser::scan(parser, syms);

    CHECK_EQ(syms.symbolCount(), size_t(1), "1 symbol registered");
    auto &data = syms.get(0);
    CHECK(data.name == "Point", "symbol name is 'Point'");
    CHECK_EQ(data.visibility, SymbolVisibility::Public, "struct visibility is Public");
}

int main() {
    std::printf("import-basics tests\n");
    std::printf("======================\n\n");

    test_parse_from_import();
    test_parse_import_single();
    test_syms_declare_visibility();
    test_scanner_pub_visibility();
    test_scanner_mod_default();
    test_scanner_mod_n_depth();
    test_scanner_mod_infinite_depth();
    test_scanner_private_default();
    test_scanner_visibility_resets();
    test_scanner_pub_struct();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
