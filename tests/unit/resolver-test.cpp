#include "../test-common.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/import-manager.hpp"
#include "import/resolver.hpp"
#include "import/symbol-id.hpp"
#include "import/symbol-table.hpp"
#include "import/symbol-visibility.hpp"
#include "memory/arena.hpp"
#include "memory/source-map.hpp"
#include "support/platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using namespace zith::ast;
using namespace zith::import;
using namespace zith::diagnostics;
using zith::memory::Arena;
using zith::memory::DynArray;
using zith::memory::SourceMap;

static SourceMap source_map;

// ── Temp directory helper for import tests ──────────────────────
static struct Cleanup {
    std::vector<std::string> dirs;
    ~Cleanup() {
        for (auto &d : dirs) {
            std::error_code ec;
            fs::remove_all(d, ec);
        }
    }
} cleanup;

static std::string make_tmp_dir() {
    auto base        = fs::temp_directory_path();
    std::string tmpl = (base / "zith_resolver_XXXXXX").string();
    char *d          = zith::support::mkdtemp(tmpl.data());
    if (d)
        cleanup.dirs.push_back(d);
    return d ? std::string(d) : std::string{};
}

// ── Helpers to avoid repeating the setup pattern ────────────────

struct ResolverFixture {
    Arena arena;
    DiagnosticEngine diags;
    AstBuilder builder;
    SymbolTable syms;
    ImportManager import_mgr;

    ResolverFixture()
        : diags(arena), builder(arena), syms(arena), import_mgr(arena, source_map, diags, {}) {}

    ResolverFixture(std::vector<std::string> roots)
        : diags(arena), builder(arena), syms(arena),
          import_mgr(arena, source_map, diags, std::move(roots)) {}

    Resolver makeResolver() {
        return Resolver(syms, import_mgr, builder, diags);
    }

    // Build a program containing a single fn decl wrapping an expression
    // fn __test() { <body_expr> }
    // Returns (program, body_expr_id)
    struct BuildResult {
        ProgramNode program;
        ExprId body_ident;
    };

    BuildResult buildFnWrapping(ExprId inner_expr) {
        DynArray<StmtId> stmts(arena);
        auto block_expr = builder.block(std::move(stmts), inner_expr);

        DynArray<std::string_view> params(arena);
        auto fn_decl = builder.fnDecl("__test", std::move(params), block_expr);

        ProgramNode program(arena);
        program.decls.push(fn_decl);
        return {std::move(program), inner_expr};
    }

    BuildResult buildFnWrappingBlock(ExprId block_expr) {
        DynArray<std::string_view> params(arena);
        auto fn_decl = builder.fnDecl("__test", std::move(params), block_expr);

        ProgramNode program(arena);
        program.decls.push(fn_decl);
        return {std::move(program), block_expr};
    }
};

// ══════════════════════════════════════════════════════════════════
// Test 1: Unqualified ident resolution
// ══════════════════════════════════════════════════════════════════
static void test_unqualified_ident() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // Declare "greet" in root scope
    auto greet_sym = fx.syms.declare("greet", SymbolVisibility::Public, 0, SymKind::Fn);

    // Build: fn __test() { greet }
    auto greet_ident      = fx.builder.ident("greet");
    auto [prog, ident_id] = fx.buildFnWrapping(greet_ident);

    resolver.resolveProgram(prog);

    CHECK(resolver.hasResolvedSym(ident_id), "greet has resolved sym");
    CHECK_EQ(resolver.resolvedSym(ident_id), greet_sym, "greet resolves to correct SymId");
}

// ══════════════════════════════════════════════════════════════════
// Test 2: Undefined ident produces diagnostic
// ══════════════════════════════════════════════════════════════════
static void test_undefined_ident_diag() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // Build: fn __test() { nope }
    auto nope_ident       = fx.builder.ident("nope");
    auto [prog, ident_id] = fx.buildFnWrapping(nope_ident);

    resolver.resolveProgram(prog);

    CHECK(!resolver.hasResolvedSym(ident_id), "nope has no resolved sym");
    CHECK_EQ(resolver.resolvedSym(ident_id), kInvalidSym, "nope resolution is invalid");

    // Check diagnostics
    auto all_diags = fx.diags.all();
    CHECK_EQ(all_diags.size(), size_t(1), "one diagnostic produced");
    if (all_diags.size() >= 1) {
        CHECK_EQ(static_cast<int>(all_diags[0].severity), static_cast<int>(Severity::Error),
                 "severity is Error");
        CHECK_EQ(all_diags[0].code, err::UndefinedIdent, "code is UndefinedIdent");
    }
}

// ══════════════════════════════════════════════════════════════════
// Test 3: Qualified name resolution
// ══════════════════════════════════════════════════════════════════
static void test_qualified_name() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // sym 0: module "mymod" in root scope
    fx.syms.declare("mymod", SymbolVisibility::Public, 0, SymKind::Module);
    // sym 1: fn "greet" in root scope
    auto greet_sym = fx.syms.declare("greet", SymbolVisibility::Public, 0, SymKind::Fn);

    // Build: fn __test() { mymod.greet }
    auto ident_id  = fx.builder.ident("mymod.greet");
    auto [prog, _] = fx.buildFnWrapping(ident_id);

    resolver.resolveProgram(prog);

    CHECK(resolver.hasResolvedSym(ident_id), "qualified ident has resolved sym");
    CHECK_EQ(resolver.resolvedSym(ident_id), greet_sym, "mymod.greet resolves to greet sym");
}

// ══════════════════════════════════════════════════════════════════
// Test 4: FnDecl scope — params visible inside body
// ══════════════════════════════════════════════════════════════════
static void test_fn_scope_params() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // Build: fn test(x) { x }
    auto x_ident = fx.builder.ident("x");

    DynArray<StmtId> stmts(fx.arena);
    auto block_expr = fx.builder.block(std::move(stmts), x_ident);

    DynArray<std::string_view> params(fx.arena);
    params.push("x");
    auto fn_decl = fx.builder.fnDecl("test", std::move(params), block_expr);

    ProgramNode program(fx.arena);
    program.decls.push(fn_decl);

    resolver.resolveProgram(program);

    // "x" should be resolved to the param declared in fn scope
    CHECK(resolver.hasResolvedSym(x_ident), "x ident in fn body has resolved sym");
    auto sym_id = resolver.resolvedSym(x_ident);
    CHECK(sym_id != kInvalidSym, "x resolved to valid sym");
    if (sym_id != kInvalidSym) {
        auto &data = fx.syms.get(sym_id);
        CHECK(data.name == "x", "resolved sym name is 'x'");
        // The Resolver declares params with SymKind::Variable (default)
        CHECK_EQ(static_cast<int>(data.kind), static_cast<int>(SymKind::Variable),
                 "param sym kind is Variable");
    }
}

// ══════════════════════════════════════════════════════════════════
// Test 5: Block scope — outer identifier visible from let init
// ══════════════════════════════════════════════════════════════════
static void test_block_scope_outer() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // Declare "outer" in root scope
    auto outer_sym = fx.syms.declare("outer", SymbolVisibility::Public, 0, SymKind::Variable);

    // Build: fn __test() { let x = outer }
    auto outer_ident = fx.builder.ident("outer");
    auto let_stmt    = fx.builder.letStmt("x", false, outer_ident);

    DynArray<StmtId> stmts(fx.arena);
    stmts.push(let_stmt);
    auto block_expr = fx.builder.block(std::move(stmts));

    DynArray<std::string_view> params(fx.arena);
    auto fn_decl = fx.builder.fnDecl("__test", std::move(params), block_expr);

    ProgramNode program(fx.arena);
    program.decls.push(fn_decl);

    resolver.resolveProgram(program);

    // "outer" should resolve to the root sym
    CHECK(resolver.hasResolvedSym(outer_ident), "outer ident has resolved sym");
    CHECK_EQ(resolver.resolvedSym(outer_ident), outer_sym, "outer resolves to root sym");
}

// ══════════════════════════════════════════════════════════════════
// Test 6: Compound expressions — all sub-expressions visited
// ══════════════════════════════════════════════════════════════════
static void test_compound_expressions() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // Declare idents in root scope
    auto a_sym = fx.syms.declare("a", SymbolVisibility::Public, 0, SymKind::Variable);
    auto b_sym = fx.syms.declare("b", SymbolVisibility::Public, 0, SymKind::Fn);
    auto c_sym = fx.syms.declare("c", SymbolVisibility::Public, 0, SymKind::Variable);

    // Build: fn __test() { a + b(c) }
    auto a_ident = fx.builder.ident("a");
    auto b_ident = fx.builder.ident("b");
    auto c_ident = fx.builder.ident("c");

    DynArray<ExprId> call_args(fx.arena);
    call_args.push(c_ident);
    auto call_expr = fx.builder.call(b_ident, std::move(call_args));

    auto binary_expr = fx.builder.binary(a_ident, BinaryOp::Add, call_expr);

    DynArray<StmtId> stmts(fx.arena);
    auto block_expr = fx.builder.block(std::move(stmts), binary_expr);

    DynArray<std::string_view> params(fx.arena);
    auto fn_decl = fx.builder.fnDecl("__test", std::move(params), block_expr);

    ProgramNode program(fx.arena);
    program.decls.push(fn_decl);

    resolver.resolveProgram(program);

    // All idents should be resolved
    CHECK(resolver.hasResolvedSym(a_ident), "a ident resolved");
    CHECK_EQ(resolver.resolvedSym(a_ident), a_sym, "a resolves correctly");

    CHECK(resolver.hasResolvedSym(b_ident), "b ident resolved");
    CHECK_EQ(resolver.resolvedSym(b_ident), b_sym, "b resolves correctly");

    CHECK(resolver.hasResolvedSym(c_ident), "c ident resolved");
    CHECK_EQ(resolver.resolvedSym(c_ident), c_sym, "c resolves correctly");

    // Also verify the binary and call exprs are NOT in resolved_ (they aren't idents)
    CHECK(!resolver.hasResolvedSym(binary_expr), "binary expr not in resolved table");
    CHECK(!resolver.hasResolvedSym(call_expr), "call expr not in resolved table");
}

// ══════════════════════════════════════════════════════════════════
// Test 7: Alias registration from imports
// ══════════════════════════════════════════════════════════════════
static void test_alias_from_imports() {
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    // Create a mymod.zith file
    {
        std::ofstream f(tmp_dir + "/mymod.zith");
        f << "pub fn greet() {}\n";
    }

    ResolverFixture fx({tmp_dir});

    // Resolve "mymod" so it's loaded in the ImportManager
    DynArray<std::string_view> resolve_path(fx.arena);
    resolve_path.push("mymod");
    auto res = fx.import_mgr.resolve(resolve_path);
    CHECK(res.isOk(), "resolve mymod");
    if (!res.isOk())
        return;

    CHECK(fx.import_mgr.isLoaded("mymod"), "mymod is loaded after resolve");

    // Register "mymod" as a module in syms (Resolver expects this)
    fx.syms.declare("mymod", SymbolVisibility::Public, 0, SymKind::Module);

    // Build program: import mymod as m
    DynArray<std::string_view> import_path(fx.arena);
    import_path.push("mymod");
    auto import_decl = fx.builder.importDecl(std::move(import_path), "m",
                                             /*is_from=*/false, /*is_export=*/false,
                                             /*import_depth=*/1);

    ProgramNode program(fx.arena);
    program.decls.push(import_decl);

    // Create resolver and run
    auto resolver = fx.makeResolver();
    resolver.resolveProgram(program);

    // Check that alias "m" was declared in syms
    auto alias_sym = fx.syms.lookup("m");
    CHECK(alias_sym != kInvalidSym, "alias 'm' was declared in syms");
    if (alias_sym != kInvalidSym) {
        auto &data = fx.syms.get(alias_sym);
        CHECK(data.name == "m", "alias name is 'm'");
        CHECK_EQ(static_cast<int>(data.kind), static_cast<int>(SymKind::Alias),
                 "alias has kind Alias");
        CHECK_EQ(static_cast<int>(data.visibility), static_cast<int>(SymbolVisibility::Public),
                 "alias visibility is Public");
    }
}

// ══════════════════════════════════════════════════════════════════
// Test 8: Alias not registered when import is not loaded
// ══════════════════════════════════════════════════════════════════
static void test_alias_not_registered_for_unloaded_import() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // Declare "mymod" in syms
    fx.syms.declare("mymod", SymbolVisibility::Public, 0, SymKind::Module);

    // Build program: import mymod as m  (but mymod is NOT resolved in ImportManager)
    DynArray<std::string_view> import_path(fx.arena);
    import_path.push("mymod");
    auto import_decl = fx.builder.importDecl(std::move(import_path), "m",
                                             /*is_from=*/false, /*is_export=*/false,
                                             /*import_depth=*/1);

    ProgramNode program(fx.arena);
    program.decls.push(import_decl);

    // Verify import is NOT loaded
    CHECK(!fx.import_mgr.isLoaded("mymod"), "mymod is NOT loaded by default");

    resolver.resolveProgram(program);

    // Alias "m" should NOT be declared (import not loaded)
    auto alias_sym = fx.syms.lookup("m");
    CHECK_EQ(alias_sym, kInvalidSym, "alias 'm' NOT declared when import not loaded");
}

// ══════════════════════════════════════════════════════════════════
// Test 9: Alias not registered when alias is empty
// ══════════════════════════════════════════════════════════════════
static void test_no_alias_for_unnamed_import() {
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/other.zith");
        f << "pub fn foo() {}\n";
    }

    ResolverFixture fx({tmp_dir});

    DynArray<std::string_view> resolve_path(fx.arena);
    resolve_path.push("other");
    auto res = fx.import_mgr.resolve(resolve_path);
    CHECK(res.isOk(), "resolve other");
    if (!res.isOk())
        return;

    fx.syms.declare("other", SymbolVisibility::Public, 0, SymKind::Module);

    // Build program: import other  (no alias)
    DynArray<std::string_view> import_path(fx.arena);
    import_path.push("other");
    auto import_decl = fx.builder.importDecl(std::move(import_path), {},
                                             /*is_from=*/false, /*is_export=*/false,
                                             /*import_depth=*/1);

    ProgramNode program(fx.arena);
    program.decls.push(import_decl);

    auto resolver = fx.makeResolver();
    resolver.resolveProgram(program);

    // No alias "other" should be created as an Alias (original module still there)
    auto other_sym = fx.syms.lookup("other");
    CHECK(other_sym != kInvalidSym, "other module still in syms");
    if (other_sym != kInvalidSym) {
        auto &data = fx.syms.get(other_sym);
        CHECK_EQ(static_cast<int>(data.kind), static_cast<int>(SymKind::Module),
                 "other is still Module, not overridden by Alias");
    }
}

// ══════════════════════════════════════════════════════════════════
// Test 10: hasResolvedSym returns false for kInvalidExpr
// ══════════════════════════════════════════════════════════════════
static void test_invalid_expr_not_resolved() {
    ResolverFixture fx;
    auto resolver = fx.makeResolver();

    // Calling hasResolvedSym/resolvedSym with kInvalidExpr should not crash
    CHECK(!resolver.hasResolvedSym(kInvalidExpr), "hasResolvedSym(kInvalidExpr) returns false");
    CHECK_EQ(resolver.resolvedSym(kInvalidExpr), kInvalidSym,
             "resolvedSym(kInvalidExpr) returns kInvalidSym");
}

// ══════════════════════════════════════════════════════════════════
// Test runner
// ══════════════════════════════════════════════════════════════════
int main() {
    std::printf("resolver tests\n");
    std::printf("====================\n\n");

    test_unqualified_ident();
    test_undefined_ident_diag();
    test_qualified_name();
    test_fn_scope_params();
    test_block_scope_outer();
    test_compound_expressions();
    test_alias_from_imports();
    test_alias_not_registered_for_unloaded_import();
    test_no_alias_for_unnamed_import();
    test_invalid_expr_not_resolved();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
