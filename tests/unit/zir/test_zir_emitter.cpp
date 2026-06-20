#include "../../test-common.hpp"
#include "zir/zir/zir-emitter.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/hir/hir-expr.hpp"
#include "memory/arena.hpp"

using namespace zith::zir;
using namespace zith::zir::hir;
using namespace zith::types;
using zith::memory::Arena;

static void test_emitter_add_const_int() {
    Arena arena;
    HirModule hir(arena);
    ZirModule mod;
    ZirEmitter emitter(hir, mod);

    ZirIndex idx1 = emitter.addConst(int64_t{42});
    ZirIndex idx2 = emitter.addConst(int64_t{99});

    CHECK_EQ(idx1, 0u, "first int const gets index 0");
    CHECK_EQ(idx2, 1u, "second int const gets index 1");
    CHECK_EQ(mod.constants.size(), 2u, "module has 2 constants");
    CHECK_EQ(std::get<int64_t>(mod.constants[0]), 42, "first const is 42");
    CHECK_EQ(std::get<int64_t>(mod.constants[1]), 99, "second const is 99");
}

static void test_emitter_add_const_double() {
    Arena arena;
    HirModule hir(arena);
    ZirModule mod;
    ZirEmitter emitter(hir, mod);

    ZirIndex idx = emitter.addConst(3.14);

    CHECK_EQ(idx, 0u, "double const gets index 0");
    CHECK_EQ(mod.constants.size(), 1u, "module has 1 constant");
    CHECK(std::holds_alternative<double>(mod.constants[0]), "const is double");
}

static void test_emitter_add_const_string() {
    Arena arena;
    HirModule hir(arena);
    ZirModule mod;
    ZirEmitter emitter(hir, mod);

    ZirIndex idx = emitter.addConst("hello");

    CHECK_EQ(idx, 0u, "string const gets index 0");
    CHECK_EQ(mod.constants.size(), 1u, "module has 1 constant");
    CHECK(std::holds_alternative<std::string>(mod.constants[0]), "const is string");
    CHECK_EQ(std::get<std::string>(mod.constants[0]), "hello", "const is hello");
}

static void test_emitter_mixed_constants() {
    Arena arena;
    HirModule hir(arena);
    ZirModule mod;
    ZirEmitter emitter(hir, mod);

    emitter.addConst(int64_t{1});
    emitter.addConst(2.5);
    emitter.addConst("str");
    emitter.addConst(int64_t{3});

    CHECK_EQ(mod.constants.size(), 4u, "module has 4 constants");
    CHECK_EQ(std::get<int64_t>(mod.constants[0]), 1, "const 0 is 1");
}

static void test_emitter_emit_empty_module() {
    Arena arena;
    HirModule hir(arena);
    ZirModule mod;
    ZirEmitter emitter(hir, mod);

    emitter.emit();

    CHECK_EQ(mod.functions.size(), 0u, "empty module emits no functions");
}

static void test_emitter_emit_single_function() {
    Arena arena;
    HirModule hir(arena);
    auto &fn = hir.addFn("main");
    fn.return_type = kVoidType;

    ZirModule mod;
    ZirEmitter emitter(hir, mod);
    emitter.emit();

    CHECK_EQ(mod.functions.size(), 1u, "module has 1 function");
    CHECK_EQ(mod.functions[0].name, "main", "function name is main");
}

static void test_zir_emitter() {
    test_emitter_add_const_int();
    test_emitter_add_const_double();
    test_emitter_add_const_string();
    test_emitter_mixed_constants();
    test_emitter_emit_empty_module();
    test_emitter_emit_single_function();
}

TEST_MAIN(zir_emitter)