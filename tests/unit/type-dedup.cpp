#include "../test-common.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "memory/arena.hpp"
#include "types/type-id.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"

using namespace zith::types;
using zith::diagnostics::DiagnosticEngine;
using zith::memory::Arena;

// ── basic dedup tests ────────────────────────────────────────────────

static void test_dedup_int_same() {
    Arena arena;
    TypeIntern types(arena);

    auto a = types.internInt(IntWidth::I32);
    auto b = types.internInt(IntWidth::I32);
    CHECK_EQ(a, b, "same Int width dedup");
}

static void test_dedup_int_different() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);
    CHECK(i32 != i64, "different Int width produces different ids");
}

static void test_dedup_float_same() {
    Arena arena;
    TypeIntern types(arena);

    auto a = types.internFloat(FloatWidth::F64);
    auto b = types.internFloat(FloatWidth::F64);
    CHECK_EQ(a, b, "same Float width dedup");
}

static void test_dedup_float_different() {
    Arena arena;
    TypeIntern types(arena);

    auto f32 = types.internFloat(FloatWidth::F32);
    auto f64 = types.internFloat(FloatWidth::F64);
    CHECK(f32 != f64, "different Float width produces different ids");
}

static void test_dedup_ptr_same() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto a   = types.internPtr(i32);
    auto b   = types.internPtr(i32);
    CHECK_EQ(a, b, "same pointee Ptr dedup");
}

static void test_dedup_ptr_different() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);
    auto a   = types.internPtr(i32);
    auto b   = types.internPtr(i64);
    CHECK(a != b, "different pointee Ptr produces different ids");
}

static void test_dedup_array_same() {
    Arena arena;
    TypeIntern types(arena);

    auto u8 = types.internInt(IntWidth::U8);
    auto a  = types.internArray(u8, 4);
    auto b  = types.internArray(u8, 4);
    CHECK_EQ(a, b, "same elem+count Array dedup");
}

static void test_dedup_array_different_count() {
    Arena arena;
    TypeIntern types(arena);

    auto u8 = types.internInt(IntWidth::U8);
    auto a  = types.internArray(u8, 4);
    auto b  = types.internArray(u8, 8);
    CHECK(a != b, "different count Array produces different ids");
}

static void test_dedup_array_different_elem() {
    Arena arena;
    TypeIntern types(arena);

    auto u8  = types.internInt(IntWidth::U8);
    auto i32 = types.internInt(IntWidth::I32);
    auto a   = types.internArray(u8, 4);
    auto b   = types.internArray(i32, 4);
    CHECK(a != b, "different elem Array produces different ids");
}

static void test_dedup_fn_same() {
    Arena arena;
    TypeIntern types(arena);

    auto i32   = types.internInt(IntWidth::I32);
    auto void_ = kVoidType;

    TypeId p1[] = {i32, i32};
    TypeId p2[] = {i32, i32};
    auto a      = types.internFn(p1, void_);
    auto b      = types.internFn(p2, void_);
    CHECK_EQ(a, b, "identical fn types dedup");
}

static void test_dedup_fn_different_ret() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);

    TypeId p1[] = {i32, i32};
    TypeId p2[] = {i32, i32};
    auto a      = types.internFn(p1, i32);
    auto b      = types.internFn(p2, i64);
    CHECK(a != b, "different return type fn produces different ids");
}

static void test_dedup_fn_different_param_count() {
    Arena arena;
    TypeIntern types(arena);

    auto i32   = types.internInt(IntWidth::I32);
    auto void_ = kVoidType;

    TypeId p1[] = {i32};
    TypeId p2[] = {i32, i32};
    auto a      = types.internFn(p1, void_);
    auto b      = types.internFn(p2, void_);
    CHECK(a != b, "different param count fn produces different ids");
}

static void test_dedup_optional_same() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto a   = types.internOptional(i32);
    auto b   = types.internOptional(i32);
    CHECK_EQ(a, b, "same inner Optional dedup");
}

static void test_dedup_optional_different() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);
    auto a   = types.internOptional(i32);
    auto b   = types.internOptional(i64);
    CHECK(a != b, "different inner Optional produces different ids");
}

static void test_dedup_failable_same() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto a   = types.internFailable(i32);
    auto b   = types.internFailable(i32);
    CHECK_EQ(a, b, "same inner Failable dedup");
}

static void test_dedup_failable_different() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);
    auto a   = types.internFailable(i32);
    auto b   = types.internFailable(i64);
    CHECK(a != b, "different inner Failable produces different ids");
}

static void test_dedup_int_unsigned() {
    Arena arena;
    TypeIntern types(arena);

    auto u32 = types.internInt(IntWidth::U32);
    auto i32 = types.internInt(IntWidth::I32);
    CHECK(u32 != i32, "U32 and I32 are distinct types");
}

static void test_dedup_never_does_not_grow() {
    Arena arena;
    TypeIntern types(arena);

    // predefined types: Error, Never, Void, Bool, Char
    CHECK_EQ(types.count(), size_t(5), "5 predefined types");
}

// ── struct dedup via defineStruct ────────────────────────────────────

static void test_dedup_struct_same_name() {
    Arena arena;
    TypeIntern types(arena);

    auto a = types.defineStruct("Point");
    types.addField(a, "x", types.internInt(IntWidth::I32));
    types.addField(a, "y", types.internInt(IntWidth::I32));

    auto b = types.intern(TypeStruct{std::get<TypeStruct>(types.lookup(a)).def_id});
    // same def_id → same TypeStruct → dedup
    CHECK_EQ(b, a, "intern with same def_id returns existing id");
}

static void test_dedup_struct_distinct() {
    Arena arena;
    TypeIntern types(arena);

    auto a = types.defineStruct("Point");
    auto b = types.defineStruct("Rect");
    CHECK(a != b, "different struct definitions produce different ids");
}

// ── type var never dedup ─────────────────────────────────────────────

static void test_dedup_type_var_not_deduped() {
    Arena arena;
    TypeIntern types(arena);

    auto v1 = types.internTypeVar();
    auto v2 = types.internTypeVar();
    CHECK(v1 != v2, "fresh type vars are distinct (never dedup)");
    CHECK(v2 > v1, "second type var id is larger");
}

int main() {
    std::printf("type-dedup tests\n");
    std::printf("===================\n\n");

    // Basic dedup
    test_dedup_int_same();
    test_dedup_int_different();
    test_dedup_float_same();
    test_dedup_float_different();
    test_dedup_ptr_same();
    test_dedup_ptr_different();
    test_dedup_array_same();
    test_dedup_array_different_count();
    test_dedup_array_different_elem();
    test_dedup_fn_same();
    test_dedup_fn_different_ret();
    test_dedup_fn_different_param_count();
    test_dedup_optional_same();
    test_dedup_optional_different();
    test_dedup_failable_same();
    test_dedup_failable_different();
    test_dedup_int_unsigned();
    test_dedup_never_does_not_grow();

    // Struct dedup
    test_dedup_struct_same_name();
    test_dedup_struct_distinct();

    // Type vars
    test_dedup_type_var_not_deduped();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
