#include "diagnostics/diagnostic-engine.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/hir/hir-expr.hpp"
#include "zir/mir/mir-module.hpp"
#include "zir/mir/mir-inst.hpp"
#include "zir/mir/mir-lower-from-hir.hpp"
#include "zir/mir/mir-verify.hpp"
#include "memory/arena.hpp"

#include <cstdio>

static int failed = 0;
static int passed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); failed++; } \
    else { std::printf("  PASS: %s\n", msg); passed++; } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

using namespace zith::zir::hir;
using namespace zith::zir::mir;
using namespace zith::types;
using zith::memory::Arena;
using zith::diagnostics::DiagnosticEngine;

static void test_hir_module_create() {
    Arena arena;
    HirModule hir(arena);

    auto &fn = hir.addFn("main");
    CHECK(fn.name == "main", "hir fn name is main");
    CHECK_EQ(fn.params.size(), size_t(0), "no params");

    auto &fn_back = hir.getFn(0);
    CHECK(fn_back.name == "main", "getFn back is main");
}

static void test_hir_add_expr() {
    Arena arena;
    HirModule hir(arena);

    auto lit_id = hir.addExpr(HirLiteral{.type = kIntType, .i = 42});
    CHECK(lit_id != kInvalidHirExpr, "addExpr literal returns valid id");

    auto &expr = hir.getExpr(lit_id);
    CHECK(std::holds_alternative<HirLiteral>(expr), "expr is HirLiteral");
    if (!std::holds_alternative<HirLiteral>(expr)) return;

    auto &lit = std::get<HirLiteral>(expr);
    CHECK_EQ(lit.i, 42, "literal value is 42");
}

static void test_hir_binary_expr() {
    Arena arena;
    HirModule hir(arena);

    auto lhs = hir.addExpr(HirLiteral{.type = kIntType, .i = 1});
    auto rhs = hir.addExpr(HirLiteral{.type = kIntType, .i = 2});
    auto bin = hir.addExpr(HirBinary{lhs, rhs, HirBinaryOp::Add});
    CHECK(bin != kInvalidHirExpr, "addExpr binary returns valid id");
}

static void test_hir_module_add_fn() {
    Arena arena;
    HirModule hir(arena);

    hir.addFn("foo");
    hir.addFn("bar");
    hir.addFn("baz");

    CHECK_EQ(hir.getFn(1).name, "bar", "second fn is bar");
}

static void test_mir_module_create() {
    Arena arena;
    MirModule mir(arena);

    auto &fn = mir.addFn("main");
    CHECK(fn.name == "main", "mir fn name is main");
    CHECK_EQ(mir.fnCount(), size_t(1), "one function");
}

static void test_mir_lower_simple() {
    Arena arena;
    HirModule hir(arena);
    DiagnosticEngine diags;

    auto main_id = hir.addFn("main").name;
    (void)main_id;

    auto lit = hir.addExpr(HirLiteral{.type = kIntType, .i = 7});
    hir.addExpr(HirRet{lit});

    MirLowering lower(hir, arena, diags);
    auto mir = lower.lower();

    CHECK_EQ(mir.fnCount(), size_t(1), "lowered has one fn");
    CHECK(mir.getFn(0).name == "main", "lowered fn name is main");
}

static void test_mir_verify() {
    Arena arena;
    HirModule hir(arena);
    DiagnosticEngine diags;

    hir.addFn("test_fn");

    MirLowering lower(hir, arena, diags);
    auto mir = lower.lower();

    MirVerifier verifier(mir, diags);
    CHECK(verifier.verify(), "mir verify passes");
}

int main() {
    std::printf("mir-lowering tests\n");
    std::printf("======================\n\n");

    test_hir_module_create();
    test_hir_add_expr();
    test_hir_binary_expr();
    test_hir_module_add_fn();
    test_mir_module_create();
    test_mir_lower_simple();
    test_mir_verify();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
