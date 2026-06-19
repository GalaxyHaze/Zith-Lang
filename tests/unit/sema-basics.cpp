#include "../test-common.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "import/symbol-id.hpp"
#include "import/symbol-table.hpp"
#include "memory/arena.hpp"
#include "types/type-id.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"
#include "types/unify.hpp"

#include <span>

using namespace zith::import;
using namespace zith::types;
using zith::diagnostics::DiagnosticEngine;
using zith::memory::Arena;

// ── SymbolTable tests ─────────────────────────────────────────────

static void test_symtab_declare() {
    Arena arena;
    SymbolTable syms(arena);

    auto id = syms.declare("x");
    CHECK(id != kInvalidSym, "declare returns valid SymId");
    CHECK_EQ(syms.get(id).name, "x", "declared symbol name is 'x'");
}

static void test_symtab_lookup_finds_declared() {
    Arena arena;
    SymbolTable syms(arena);

    auto id    = syms.declare("x");
    auto found = syms.lookup("x");
    CHECK_EQ(found, id, "lookup finds declared symbol");
}

static void test_symtab_lookup_not_found() {
    Arena arena;
    SymbolTable syms(arena);

    auto not_found = syms.lookup("nonexistent");
    CHECK_EQ(not_found, kInvalidSym, "lookup nonexistent returns invalid");
}

static void test_symtab_scopes() {
    Arena arena;
    SymbolTable syms(arena);

    auto root_scope = syms.currentScope();
    CHECK_EQ(root_scope, kRootScope, "initial scope is root");

    auto scope = syms.enterScope();
    CHECK(scope != kInvalidScope, "enterScope returns valid");
    CHECK(scope != root_scope, "new scope differs from root");

    syms.exitScope();
    CHECK_EQ(syms.currentScope(), kRootScope, "after exit, back to root");
}

static void test_symtab_scope_hides_outer() {
    Arena arena;
    SymbolTable syms(arena);

    syms.declare("x");
    syms.enterScope();
    syms.declare("x");
    auto found = syms.lookup("x");
    CHECK(found != kInvalidSym, "inner x found");
    CHECK_EQ(syms.get(found).name, "x", "inner x name correct");
    syms.exitScope();
}

// ── TypeIntern tests ──────────────────────────────────────────────

static void test_type_intern_predefined_ids() {
    Arena arena;
    TypeIntern types(arena);

    CHECK_EQ(types.count(), size_t(5), "5 predefined types seeded");
    CHECK_EQ(types.kindOf(kErrorType), TypeKind::Error, "0 → Error");
    CHECK_EQ(types.kindOf(kNeverType), TypeKind::Never, "1 → Never");
    CHECK_EQ(types.kindOf(kVoidType),  TypeKind::Void,  "2 → Void");
    CHECK_EQ(types.kindOf(kBoolType),  TypeKind::Bool,  "3 → Bool");
    CHECK_EQ(types.kindOf(kCharType),  TypeKind::Char,  "4 → Char");
}

static void test_type_intern_int() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    CHECK(i32 != kErrorType, "internInt returns valid id");
    CHECK(i32 >= kFirstCustom, "internInt id >= kFirstCustom");
    CHECK_EQ(types.kindOf(i32), TypeKind::Int, "kindOf is Int");

    auto i32_b = types.internInt(IntWidth::I32);
    CHECK_EQ(i32_b, i32, "dedup — same Int(I32) returns existing id");
    CHECK_EQ(types.count(), size_t(6), "only 1 new int type added (dedup)");

    auto i64 = types.internInt(IntWidth::I64);
    CHECK(i64 != i32, "different width creates new id");
    CHECK_EQ(types.count(), size_t(7), "2 int types total after dedup");
}

static void test_type_intern_float() {
    Arena arena;
    TypeIntern types(arena);

    auto f64 = types.internFloat(FloatWidth::F64);
    CHECK(f64 != kErrorType, "internFloat returns valid id");
    CHECK_EQ(types.kindOf(f64), TypeKind::Float, "kindOf is Float");
}

static void test_type_intern_ptr() {
    Arena arena;
    TypeIntern types(arena);

    auto i32   = types.internInt(IntWidth::I32);
    auto ptr   = types.internPtr(i32);
    CHECK_EQ(types.kindOf(ptr), TypeKind::Ptr, "kindOf is Ptr");

    auto &data = std::get<TypePtr>(types.lookup(ptr));
    CHECK_EQ(data.pointee, i32, "pointee is i32");
}

static void test_type_intern_array() {
    Arena arena;
    TypeIntern types(arena);

    auto u8   = types.internInt(IntWidth::U8);
    auto arr  = types.internArray(u8, 4);
    CHECK_EQ(types.kindOf(arr), TypeKind::Array, "kindOf is Array");

    auto &data = std::get<TypeArray>(types.lookup(arr));
    CHECK_EQ(data.elem,  u8,  "elem is u8");
    CHECK_EQ(data.count, 4u,  "count is 4");
}

static void test_type_intern_slice() {
    Arena arena;
    TypeIntern types(arena);

    auto u8   = types.internInt(IntWidth::U8);
    auto sl   = types.internArray(u8, 0);
    CHECK_EQ(types.kindOf(sl), TypeKind::Array, "kindOf is Array (count=0 = slice)");

    auto &data = std::get<TypeArray>(types.lookup(sl));
    CHECK_EQ(data.elem,  u8, "elem is u8");
    CHECK_EQ(data.count, 0u, "count is 0 (slice)");
}

static void test_type_intern_fn() {
    Arena arena;
    TypeIntern types(arena);

    auto i32  = types.internInt(IntWidth::I32);
    auto void_ = kVoidType;

    TypeId param_arr[] = {i32, i32};
    auto fn   = types.internFn(param_arr, void_);
    CHECK_EQ(types.kindOf(fn), TypeKind::Fn, "kindOf is Fn");

    auto &data = std::get<TypeFn>(types.lookup(fn));
    CHECK_EQ(data.ret, void_, "return type is void");
    CHECK_EQ(data.param_count, size_t(2), "2 parameters");
    CHECK_EQ(data.params[0], i32, "param[0] is i32");
    CHECK_EQ(data.params[1], i32, "param[1] is i32");
}

static void test_type_define_struct_empty() {
    Arena arena;
    TypeIntern types(arena);

    auto s = types.defineStruct("Point");
    CHECK_EQ(types.kindOf(s), TypeKind::Struct, "kindOf is Struct");
    CHECK_EQ(types.getStructDef(s).name, "Point", "struct name is Point");
    CHECK_EQ(types.fieldCount(s), size_t(0), "no fields yet");
}

static void test_type_struct_add_field() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto f64 = types.internFloat(FloatWidth::F64);

    auto s = types.defineStruct("Point");
    types.addField(s, "x", i32);
    types.addField(s, "y", i32);
    types.addField(s, "z", f64);

    CHECK_EQ(types.fieldCount(s), size_t(3), "3 fields");
    CHECK(types.hasField(s, "x"), "has field 'x'");
    CHECK(types.hasField(s, "z"), "has field 'z'");
    CHECK(!types.hasField(s, "w"), "no field 'w'");
    CHECK_EQ(types.fieldType(s, "x"), i32, "field 'x' type is i32");
    CHECK_EQ(types.fieldType(s, "z"), f64, "field 'z' type is f64");
    CHECK_EQ(types.fieldType(s, "w"), kErrorType, "missing field returns kErrorType");
}

static void test_type_struct_field_by_index() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto s   = types.defineStruct("Pair");

    types.addField(s, "first", i32);
    types.addField(s, "second", i32);

    CHECK_EQ(types.getField(s, 0).name, "first", "field 0 name");
    CHECK_EQ(types.getField(s, 0).type, i32, "field 0 type");
    CHECK_EQ(types.getField(s, 1).name, "second", "field 1 name");
}

static void test_type_struct_fields_span_across_defs() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto f64 = types.internFloat(FloatWidth::F64);

    auto p = types.defineStruct("Point");
    types.addField(p, "x", i32);

    auto r = types.defineStruct("Rect");
    types.addField(r, "tl", p);
    types.addField(r, "br", p);

    CHECK_EQ(types.fieldCount(p), size_t(1), "Point has 1 field");
    CHECK_EQ(types.fieldCount(r), size_t(2), "Rect has 2 fields");
    CHECK_EQ(types.getStructDef(p).name, "Point", "Point name intact");
    CHECK_EQ(types.getStructDef(r).name, "Rect", "Rect name intact");
}

static void test_type_intern_optional() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto opt = types.internOptional(i32);
    CHECK_EQ(types.kindOf(opt), TypeKind::Optional, "kindOf is Optional");
    CHECK_EQ(std::get<TypeOptional>(types.lookup(opt)).inner, i32, "inner is i32");
}

static void test_type_intern_failable() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto fail = types.internFailable(i32);
    CHECK_EQ(types.kindOf(fail), TypeKind::Failable, "kindOf is Failable");
    CHECK_EQ(std::get<TypeFailable>(types.lookup(fail)).inner, i32, "inner is i32");
}

static void test_type_intern_type_var() {
    Arena arena;
    TypeIntern types(arena);

    auto v1 = types.internTypeVar();
    auto v2 = types.internTypeVar();
    CHECK(v1 != v2, "each fresh type var gets unique id");
    CHECK_EQ(types.kindOf(v1), TypeKind::TypeVar, "kindOf is TypeVar");
}

static void test_kind_of_invalid() {
    Arena arena;
    TypeIntern types(arena);

    CHECK_EQ(types.kindOf(kInvalidType), TypeKind::Error, "kInvalidType → Error");
}

// ── Unifier tests ─────────────────────────────────────────────────

static void test_unify_trivial() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    CHECK(unifier.unify(i32, i32), "unify same type");
}

static void test_unify_different_primitive_fails() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);
    CHECK(!unifier.unify(i32, i64), "unify I32 != I64");
}

static void test_unify_var() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto var = unifier.freshVar();
    CHECK(unifier.unify(var, i32), "unify var with i32");
    CHECK_EQ(unifier.substitute(var), i32, "var resolved to i32");
}

static void test_unify_var_transitive() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto a   = unifier.freshVar();
    auto b   = unifier.freshVar();

    CHECK(unifier.unify(a, b), "unify a = b");
    CHECK(unifier.unify(b, i32), "unify b = i32");
    CHECK_EQ(unifier.substitute(a), i32, "a resolved through b to i32");
}

static void test_unify_occurs_check() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto a = unifier.freshVar();
    auto p = types.internPtr(a);

    CHECK(!unifier.unify(a, p), "unify a = ptr(a) fails (cyclic)");
}

static void test_unify_occurs_check_transitive() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto a = unifier.freshVar();
    auto b = unifier.freshVar();
    auto p = types.internPtr(b);

    CHECK(unifier.unify(a, p), "unify a = ptr(b) succeeds (a != b)");
    CHECK(!unifier.unify(b, a), "unify b = a fails (would make ptr(a) cyclic via a = ptr(b))");
}

static void test_unify_occurs_transitive_deep() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto a = unifier.freshVar();
    auto b = unifier.freshVar();
    auto c = unifier.freshVar();
    auto p = types.internPtr(c);

    // a = ptr(c),  b = ptr(a) → b = ptr(ptr(c))
    CHECK(unifier.unify(a, p), "a = ptr(c)");
    auto q = types.internPtr(a);
    CHECK(unifier.unify(b, q), "b = ptr(a) = ptr(ptr(c))");
    CHECK(!unifier.unify(c, b), "c = b fails (cyclic: c in b = ptr(ptr(c)))");
}

static void test_unify_fn() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto i64 = types.internInt(IntWidth::I64);

    TypeId p1[] = {i32, i32};
    TypeId p2[] = {i32, i32};
    auto fn_a = types.internFn(p1, i32);
    auto fn_b = types.internFn(p2, i32);
    CHECK(unifier.unify(fn_a, fn_b), "unify identical fn types");

    TypeId p3[] = {i32, i64};
    auto fn_c = types.internFn(p3, i32);
    CHECK(!unifier.unify(fn_a, fn_c), "unify fn with different param types fails");
}

static void test_fresh_var_unique() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto a = unifier.freshVar();
    auto b = unifier.freshVar();
    CHECK(a != b, "each freshVar returns unique id");
    CHECK(a != kErrorType, "freshVar != kErrorType");
}

static void test_assignable_same() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    CHECK(unifier.isAssignable(i32, i32), "i32 ≤ i32");
}

static void test_assignable_never() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    CHECK(unifier.isAssignable(i32, kNeverType), "Never ≤ i32 (bottom type)");
}

static void test_coercible_int_widening() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i8  = types.internInt(IntWidth::I8);
    auto i32 = types.internInt(IntWidth::I32);
    CHECK(unifier.isCoercible(i32, i8), "i8 coerces to i32");
    CHECK(!unifier.isCoercible(i8, i32), "i32 does NOT coerce to i8");
}

static void test_coercible_float_widening() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto f32 = types.internFloat(FloatWidth::F32);
    auto f64 = types.internFloat(FloatWidth::F64);
    CHECK(unifier.isCoercible(f64, f32), "f32 coerces to f64");
    CHECK(!unifier.isCoercible(f32, f64), "f64 does NOT coerce to f32");
}

static void test_unify_optional() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto a   = types.internOptional(i32);
    auto b   = types.internOptional(i32);
    CHECK(unifier.unify(a, b), "unify identical optional types");
}

static void test_unify_failable() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto a   = types.internFailable(i32);
    auto b   = types.internFailable(i32);
    CHECK(unifier.unify(a, b), "unify identical failable types");
}

int main() {
    std::printf("sema-basics tests\n");
    std::printf("====================\n\n");

    // SymbolTable
    test_symtab_declare();
    test_symtab_lookup_finds_declared();
    test_symtab_lookup_not_found();
    test_symtab_scopes();
    test_symtab_scope_hides_outer();

    // TypeIntern
    test_type_intern_predefined_ids();
    test_type_intern_int();
    test_type_intern_float();
    test_type_intern_ptr();
    test_type_intern_array();
    test_type_intern_slice();
    test_type_intern_fn();
    test_type_define_struct_empty();
    test_type_struct_add_field();
    test_type_struct_field_by_index();
    test_type_struct_fields_span_across_defs();
    test_type_intern_optional();
    test_type_intern_failable();
    test_type_intern_type_var();
    test_kind_of_invalid();

    // Unifier
    test_unify_trivial();
    test_unify_different_primitive_fails();
    test_unify_var();
    test_unify_var_transitive();
    test_unify_occurs_check();
    test_unify_occurs_check_transitive();
    test_unify_occurs_transitive_deep();
    test_unify_fn();
    test_unify_optional();
    test_unify_failable();
    test_fresh_var_unique();

    // Assignability / Coercibility
    test_assignable_same();
    test_assignable_never();
    test_coercible_int_widening();
    test_coercible_float_widening();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
