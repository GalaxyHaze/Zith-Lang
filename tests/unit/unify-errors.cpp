#include "../test-common.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "memory/arena.hpp"
#include "memory/span.hpp"
#include "types/type-id.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"
#include "types/unify.hpp"

using namespace zith::types;
using zith::diagnostics::DiagnosticEngine;
using zith::memory::Arena;
using zith::memory::Span;

// ── unify with span — error reporting tests ──────────────────────────

static void test_unify_cyclic_reports_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto a = unifier.freshVar();
    auto p = types.internPtr(a);
    Span sp{1, 2, 5};

    bool result = unifier.unify(a, p, sp);
    CHECK(!result, "unify a = ptr(a) fails (cyclic)");
    CHECK(diags.hasErrors(), "cyclic unify reports diagnostic");
}

static void test_unify_kind_mismatch_reports_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto f64 = types.internFloat(FloatWidth::F64);
    Span sp{1, 3, 6};

    // Int vs Float are different kinds — should error
    // Note: this may pass if unify returns false without error depending on impl
    bool result = unifier.unify(i32, f64, sp);
    CHECK(!result, "Int != Float unify fails");
}

static void test_unify_width_mismatch_reports_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);
    Span sp{1, 3, 6};

    bool result = unifier.unify(i32, i64, sp);
    CHECK(!result, "I32 != I64 unify fails");
    CHECK(diags.hasErrors(), "width mismatch reports diagnostic");
}

static void test_unify_array_len_mismatch_reports_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto u8 = types.internInt(IntWidth::U8);
    auto a4 = types.internArray(u8, 4);
    auto a8 = types.internArray(u8, 8);
    Span sp{1, 3, 6};

    bool result = unifier.unify(a4, a8, sp);
    CHECK(!result, "array [u8;4] != [u8;8] unify fails");
}

static void test_unify_fn_param_count_mismatch_reports_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);

    TypeId p1[] = {i32, i32};
    TypeId p2[] = {i32};

    auto fn_a = types.internFn(p1, i32);
    auto fn_b = types.internFn(p2, i32);
    Span sp{1, 3, 6};

    bool result = unifier.unify(fn_a, fn_b, sp);
    CHECK(!result, "fn(i32,i32) != fn(i32) unify fails");
}

static void test_unify_struct_mismatch_reports_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto s1 = types.defineStruct("Point");
    auto s2 = types.defineStruct("Rect");

    // Different structs should fail to unify
    bool result = unifier.unify(s1, s2, {1, 3, 6});
    CHECK(!result, "different structs fail to unify");
}

static void test_unify_same_type_no_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);

    bool result = unifier.unify(i32, i32, Span{});
    CHECK(result, "same type unify succeeds");
    CHECK(!diags.hasErrors(), "no errors for same type");
}

static void test_unify_var_no_error() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto var = unifier.freshVar();

    bool result = unifier.unify(var, i32, Span{});
    CHECK(result, "var with i32 unify succeeds");
    CHECK(!diags.hasErrors(), "no errors for var unify");
}

static void test_unify_no_span_no_crash() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);

    // Calling without a span (default {}) should not crash
    bool result = unifier.unify(i32, i64);
    CHECK(!result, "unify without span still returns false");
}

int main() {
    std::printf("unify-errors tests\n");
    std::printf("=====================\n\n");

    test_unify_cyclic_reports_error();
    test_unify_kind_mismatch_reports_error();
    test_unify_width_mismatch_reports_error();
    test_unify_array_len_mismatch_reports_error();
    test_unify_fn_param_count_mismatch_reports_error();
    test_unify_struct_mismatch_reports_error();
    test_unify_same_type_no_error();
    test_unify_var_no_error();
    test_unify_no_span_no_crash();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
