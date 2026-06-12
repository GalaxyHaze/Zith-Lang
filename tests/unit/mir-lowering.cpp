#include "../test-common.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "memory/arena.hpp"
#include "types/type-id.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"
#include "zir/hir/hir-expr.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/hir/hir-types.hpp"
#include "zir/hir/hir-verify.hpp"
#include "zir/mir/mir-inst.hpp"
#include "zir/mir/mir-lower-from-hir.hpp"
#include "zir/mir/mir-module.hpp"
#include "zir/mir/mir-verify.hpp"

using namespace zith::zir::hir;
using namespace zith::zir::mir;
using zith::diagnostics::DiagnosticEngine;
using zith::memory::Arena;
using zith::types::TypeIntern;
using zith::types::IntWidth;

static Arena global_arena;
static TypeIntern *g_types = nullptr;

static TypeIntern &testTypes() {
    if (!g_types)
        g_types = new TypeIntern(global_arena);
    return *g_types;
}

static HirLiteral lit_int(int64_t v) {
    auto i32 = testTypes().internInt(IntWidth::I32);
    return HirLiteral{i32, v};
}

static void test_hir_add_expr() {
    Arena arena;
    HirModule hir(arena);

    auto lit_id = hir.addExpr(lit_int(42));
    CHECK(lit_id != kInvalidHirExpr, "addExpr literal returns valid id");

    auto &expr = hir.getExpr(lit_id);
    CHECK(std::holds_alternative<HirLiteral>(expr), "expr is HirLiteral");
    if (!std::holds_alternative<HirLiteral>(expr))
        return;

    auto &lit = std::get<HirLiteral>(expr);
    CHECK_EQ(lit.i, 42, "literal value is 42");
}

static void test_hir_add_expr_sequential_ids() {
    Arena arena;
    HirModule hir(arena);

    auto id1 = hir.addExpr(lit_int(1));
    auto id2 = hir.addExpr(lit_int(2));
    CHECK(id2 > id1, "sequential addExpr yields increasing ids");
}

static void test_hir_binary_expr() {
    Arena arena;
    HirModule hir(arena);

    auto lhs = hir.addExpr(lit_int(1));
    auto rhs = hir.addExpr(lit_int(2));
    auto bin = hir.addExpr(HirBinary{lhs, rhs, HirBinaryOp::Add});
    CHECK(bin != kInvalidHirExpr, "addExpr binary returns valid id");
}

static void test_hir_add_fn_and_retrieve() {
    Arena arena;
    HirModule hir(arena);

    auto i32 = testTypes().internInt(IntWidth::I32);

    auto &fn        = hir.addFn("main");
    fn.return_type  = i32;
    auto &retrieved = hir.getFn(0);
    CHECK_EQ(retrieved.return_type, i32, "fn return type is i32");
}

static void test_mir_module_create() {
    Arena arena;
    MirModule mir(arena);

    auto &fn = mir.addFn("main");
    CHECK(fn.name == "main", "mir fn name is main");
    CHECK_EQ(mir.fnCount(), size_t(1), "one function in module");
}

static void test_mir_add_multiple_fn() {
    Arena arena;
    MirModule mir(arena);

    mir.addFn("foo");
    mir.addFn("bar");
    CHECK_EQ(mir.fnCount(), size_t(2), "two functions in module");
    CHECK(mir.getFn(0).name == "foo", "first fn is foo");
    CHECK(mir.getFn(1).name == "bar", "second fn is bar");
}

static void test_mir_lower_no_crash() {
    Arena arena;
    HirModule hir(arena);
    DiagnosticEngine diags(arena);

    hir.addFn("main");

    MirLowering lower(hir, arena, diags);
    auto mir = lower.lower();
    CHECK_EQ(mir.fnCount(), size_t(0), "lowered module is empty (stub)");
}

static void test_hir_verify_returns_true() {
    Arena arena;
    HirModule hir(arena);
    DiagnosticEngine diags(arena);

    HirVerifier verifier(hir, diags);
    CHECK(verifier.verify(), "hir verify returns true (stub)");
}

static void test_mir_verify_returns_true() {
    Arena arena;
    MirModule mir(arena);
    DiagnosticEngine diags(arena);

    MirVerifier verifier(mir, diags);
    CHECK(verifier.verify(), "mir verify returns true (stub)");
}

int main() {
    std::printf("mir-lowering tests\n");
    std::printf("======================\n\n");

    test_hir_add_expr();
    test_hir_add_expr_sequential_ids();
    test_hir_binary_expr();
    test_hir_add_fn_and_retrieve();
    test_mir_module_create();
    test_mir_add_multiple_fn();
    test_mir_lower_no_crash();
    test_hir_verify_returns_true();
    test_mir_verify_returns_true();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
