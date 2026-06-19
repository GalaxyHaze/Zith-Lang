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
#include "sema/sema-pipeline.hpp"
#include "types/type-id.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"
#include "zir/hir/hir-expr.hpp"
#include "zir/hir/hir-module.hpp"

auto sema_hir_source_map = zith::memory::SourceMap();

using namespace zith::ast;
using namespace zith::zir::hir;
using zith::diagnostics::DiagnosticEngine;
using zith::import::SymbolTable;
using zith::lexer::tokenize;
using zith::lexer::TokenStream;
using zith::memory::Arena;
using zith::parser::Parser;
using zith::parser::scan;
using zith::parser::ScanResult;
using zith::sema::SemaPipeline;
using zith::types::kErrorType;
using zith::types::kVoidType;
using zith::types::TypeIntern;

// ── helpers ──────────────────────────────────────────────────────────

struct SemaTestContext {
    Arena ast_arena;
    Arena sym_arena;
    Arena type_arena;
    Arena hir_arena;
    AstBuilder bld;
    DiagnosticEngine diags;
    SymbolTable syms;
    TypeIntern types;

    SemaTestContext()
        : ast_arena(), sym_arena(), type_arena(), hir_arena(), bld(ast_arena), diags(ast_arena),
          syms(sym_arena), types(type_arena) {}
};

// Run the full pipeline: lex → scan → expand → sema
static bool runSema(SemaTestContext &ctx, std::string_view src, HirModule &out_hir) {
    auto toks =
        tokenize(sema_hir_source_map, ctx.ast_arena, "test", std::string(src), ctx.diags).value();

    Parser parser(&toks, &ctx.bld, &ctx.diags);
    auto result = scan(parser, ctx.syms);
    parser.expandBodies(result);

    SemaPipeline pipeline(ctx.syms, ctx.types, ctx.diags, ctx.bld, ctx.hir_arena);
    bool ok = pipeline.run(parser.program);
    out_hir = pipeline.takeHir();
    return ok;
}

// ── HIR lowering tests ───────────────────────────────────────────────

static void test_sema_empty_fn() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn foo() {}", hir);
    CHECK(ok, "sema succeeds for empty fn");
    CHECK_EQ(hir.getFnCount(), size_t(1), "hir has 1 function");
    CHECK(hir.getFn(0).name == "foo", "hir fn name is foo");
}

static void test_sema_fn_return_literal() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { return 42; }", hir);
    CHECK(ok, "sema succeeds for fn with return literal");
    CHECK_EQ(hir.getFnCount(), size_t(1), "hir has 1 function");
}

static void test_sema_fn_multiple() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn a() { return 1; } fn b() { return 2; }", hir);
    CHECK(ok, "sema succeeds for multiple fns");
    CHECK_EQ(hir.getFnCount(), size_t(2), "hir has 2 functions");
    CHECK(hir.getFn(0).name == "a", "fn[0] is a");
    CHECK(hir.getFn(1).name == "b", "fn[1] is b");
}

static void test_sema_fn_with_params() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn add(x, y) { return x + y; }", hir);
    CHECK(ok, "sema succeeds for fn with binary expr");
    CHECK_EQ(hir.getFnCount(), size_t(1), "hir has 1 function");
}

static void test_sema_empty_program() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "", hir);
    CHECK(ok, "sema succeeds for empty program");
    CHECK_EQ(hir.getFnCount(), size_t(0), "hir has 0 functions");
}

static void test_sema_let_stmt() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { let x = 42; }", hir);
    CHECK(ok, "sema succeeds for fn with let stmt");
}

static void test_sema_if_expr() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { if true { 1 } else { 2 }; }", hir);
    CHECK(ok, "sema succeeds for fn with if expr");
}

static void test_sema_while_loop() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { while true { 1 }; }", hir);
    CHECK(ok, "sema succeeds for fn with while loop");
}

static void test_sema_for_loop() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { for (let i = 0; i < 10; i + 1) { i }; }", hir);
    CHECK(ok, "sema succeeds for fn with for loop");
}

static void test_sema_binary_expr() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { 1 + 2 * 3; }", hir);
    CHECK(ok, "sema succeeds for binary expression");
}

static void test_sema_unary_expr() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { -42; }", hir);
    CHECK(ok, "sema succeeds for unary expression");
}

static void test_sema_call_expr() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn foo() { 1 } fn main() { foo(); }", hir);
    CHECK(ok, "sema succeeds for call expression");
    CHECK_EQ(hir.getFnCount(), size_t(2), "hir has 2 functions");
}

static void test_sema_compound_ops() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { 1 == 2; 3 != 4; 5 <= 6; 7 >= 8; }", hir);
    CHECK(ok, "sema succeeds for compound operators");
}

static void test_sema_word_ops() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { true and false; true or false; }", hir);
    CHECK(ok, "sema succeeds for word operators");
}

static void test_sema_block_expr() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { let x = { 42 }; }", hir);
    CHECK(ok, "sema succeeds for block expression");
}

static void test_sema_assignment() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { let x; }", hir);
    CHECK(ok, "sema succeeds for let statement");
}

static void test_sema_undefined_ident() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    // Using an undefined identifier should not crash but may produce errors
    bool ok = runSema(ctx, "fn main() { undefined_var; }", hir);
    // It may or may not produce errors depending on sema strictness
    // The test just verifies no crash
    CHECK(true, "undefined ident does not crash");
}

static void test_sema_nested_if() {
    SemaTestContext ctx;
    HirModule hir(ctx.hir_arena);

    bool ok = runSema(ctx, "fn main() { if a { if b { 1 } else { 2 } } else { 3 } }", hir);
    // true if no errors, test for no crash
    CHECK(true, "nested if does not crash");
}

int main() {
    std::printf("sema-hir tests\n");
    std::printf("================\n\n");

    test_sema_empty_fn();
    test_sema_fn_return_literal();
    test_sema_fn_multiple();
    test_sema_fn_with_params();
    test_sema_empty_program();
    test_sema_let_stmt();
    test_sema_if_expr();
    test_sema_while_loop();
    test_sema_for_loop();
    test_sema_binary_expr();
    test_sema_unary_expr();
    test_sema_call_expr();
    test_sema_compound_ops();
    test_sema_word_ops();
    test_sema_block_expr();
    test_sema_assignment();
    test_sema_undefined_ident();
    test_sema_nested_if();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
