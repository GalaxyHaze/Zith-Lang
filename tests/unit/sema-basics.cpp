#include "import/symbol-table.hpp"
#include "import/symbol-id.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"
#include "types/type-id.hpp"
#include "types/unify.hpp"
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

using namespace zith::import;
using namespace zith::types;
using zith::memory::Arena;
using zith::diagnostics::DiagnosticEngine;

static Arena arena;

static void test_symtab_declare_lookup() {
    Arena local_arena;
    SymbolTable syms(local_arena);

    auto id = syms.declare("x");
    CHECK(id != kInvalidSym, "declare x returns valid id");

    auto found = syms.lookup("x");
    CHECK_EQ(found, id, "lookup x returns same id");

    auto not_found = syms.lookup("y");
    CHECK_EQ(not_found, kInvalidSym, "lookup y returns invalid");
}

static void test_symtab_scopes() {
    Arena local_arena;
    SymbolTable syms(local_arena);

    auto x_outer = syms.declare("x");
    CHECK(x_outer != kInvalidSym, "declare x in root");

    auto scope = syms.enterScope();
    CHECK(scope != kInvalidScope, "enterScope returns valid");

    auto x_inner = syms.declare("x");
    CHECK(x_inner != kInvalidSym, "declare x in inner scope");
    CHECK(x_inner != x_outer, "inner x different from outer x");

    auto looked = syms.lookup("x");
    CHECK_EQ(looked, x_inner, "lookup x finds inner scope first");

    syms.exitScope();
    auto after_exit = syms.lookup("x");
    CHECK_EQ(after_exit, x_outer, "after exit, lookup finds outer x");
}

static void test_type_intern_basics() {
    Arena local_arena;
    TypeIntern types(local_arena);

    auto i32 = types.internInt(IntWidth::I32);
    CHECK(i32 != kInvalidType, "internInt returns valid");

    auto f64 = types.internFloat(FloatWidth::F64);
    CHECK(f64 != kInvalidType, "internFloat returns valid");

    CHECK_EQ(types.kindOf(i32), TypeKind::Int, "kindOf i32 is Int");
    CHECK_EQ(types.kindOf(f64), TypeKind::Float, "kindOf f64 is Float");
    CHECK_EQ(types.kindOf(kBoolType), TypeKind::Bool, "kindOf bool is Bool");
}

static void test_type_intern_pointer() {
    Arena local_arena;
    TypeIntern types(local_arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto ptr = types.internPtr(i32);
    CHECK(ptr != kInvalidType, "internPtr returns valid");
    CHECK_EQ(types.kindOf(ptr), TypeKind::Ptr, "kindOf ptr is Ptr");
}

static void test_type_intern_array() {
    Arena local_arena;
    TypeIntern types(local_arena);

    auto i32 = types.internInt(IntWidth::I32);
    auto arr = types.internArray(i32, 10);
    CHECK(arr != kInvalidType, "internArray returns valid");
    CHECK_EQ(types.kindOf(arr), TypeKind::Array, "kindOf arr is Array");
}

static void test_type_unification() {
    Arena local_arena;
    TypeIntern types(local_arena);
    DiagnosticEngine diags;
    Unifier unifier(types, diags);

    auto a = types.internInt(IntWidth::I32);
    auto b = types.internInt(IntWidth::I32);
    CHECK(unifier.unify(a, b), "unify i32 with i32 succeeds");

    auto c = types.internInt(IntWidth::I64);
    CHECK(!unifier.unify(a, c), "unify i32 with i64 fails");
}

static void test_type_var_unification() {
    Arena local_arena;
    TypeIntern types(local_arena);
    DiagnosticEngine diags;
    Unifier unifier(types, diags);

    auto var = unifier.freshVar();
    CHECK(var != kInvalidType, "freshVar is valid");

    auto i32 = types.internInt(IntWidth::I32);
    CHECK(unifier.unify(var, i32), "unify var with i32 succeeds");
}

int main() {
    std::printf("sema-basics tests\n");
    std::printf("====================\n\n");

    test_symtab_declare_lookup();
    test_symtab_scopes();
    test_type_intern_basics();
    test_type_intern_pointer();
    test_type_intern_array();
    test_type_unification();
    test_type_var_unification();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
