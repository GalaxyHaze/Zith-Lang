#include "import/symbol-table.hpp"
#include "import/symbol-id.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"
#include "types/type-id.hpp"
#include "types/unify.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "memory/arena.hpp"
#include "../test-common.hpp"

using namespace zith::import;
using namespace zith::types;
using zith::memory::Arena;
using zith::diagnostics::DiagnosticEngine;

// SymbolTable is stubbed — declare() returns 0, lookup() returns kInvalidSym
static void test_symtab_declare() {
    Arena arena;
    SymbolTable syms(arena);

    auto id = syms.declare("x");
    CHECK_EQ(id, SymId(0), "declare returns 0 (stub)");
}

static void test_symtab_lookup_finds_declared() {
    Arena arena;
    SymbolTable syms(arena);

    auto id = syms.declare("x");
    auto found = syms.lookup("x");
    CHECK_EQ(found, id, "lookup finds declared symbol");
}

static void test_symtab_not_found() {
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

static void test_type_intern_returns_constant_ids() {
    Arena arena;
    TypeIntern types(arena);

    auto i32 = types.internInt(IntWidth::I32);
    CHECK_EQ(i32, kIntType, "internInt returns kIntType (stub)");

    auto f64 = types.internFloat(FloatWidth::F64);
    CHECK_EQ(f64, kFloatType, "internFloat returns kFloatType (stub)");
}

static void test_type_kind_of_returns_error() {
    Arena arena;
    TypeIntern types(arena);

    auto id = types.internInt(IntWidth::I32);
    CHECK_EQ(types.kindOf(id), TypeKind::Error, "kindOf returns Error (stub)");
}

static void test_type_intern_ptr_returns_error() {
    Arena arena;
    TypeIntern types(arena);

    auto ptr = types.internPtr(kIntType);
    CHECK_EQ(ptr, kErrorType, "internPtr returns kErrorType (stub)");
}

static void test_unifier_returns_false() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    CHECK(!unifier.unify(kIntType, kIntType), "unify returns false (stub)");
}

static void test_fresh_var_returns_error_type() {
    Arena arena;
    TypeIntern types(arena);
    DiagnosticEngine diags(arena);
    Unifier unifier(types, diags, arena);

    auto var = unifier.freshVar();
    CHECK_EQ(var, kErrorType, "freshVar returns kErrorType (stub)");
}

int main() {
    std::printf("sema-basics tests\n");
    std::printf("====================\n\n");

    test_symtab_declare();
    test_symtab_lookup_finds_declared();
    test_symtab_not_found();
    test_symtab_scopes();
    test_type_intern_returns_constant_ids();
    test_type_kind_of_returns_error();
    test_type_intern_ptr_returns_error();
    test_unifier_returns_false();
    test_fresh_var_returns_error_type();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
