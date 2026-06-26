#include "../../test-common.hpp"
#include "memory/arena.hpp"
#include "zir/zir/zir-inst.hpp"

#include <string>

using namespace zith::zir;
using namespace zith::types;
using zith::memory::Arena;

static void test_zir_inst_default_constructor() {
    ZirInst inst;
    CHECK_EQ(inst.op, ZirOp::Nop, "default inst is Nop");
    CHECK_EQ(inst.imm32, 0u, "default imm32 is 0");
    CHECK_EQ(inst.imm8, 0u, "default imm8 is 0");
}

static void test_zir_inst_constructor() {
    ZirInst inst(ZirOp::Push, 42, 7);
    CHECK_EQ(inst.op, ZirOp::Push, "inst op is Push");
    CHECK_EQ(inst.imm32, 42u, "inst imm32 is 42");
    CHECK_EQ(inst.imm8, 7u, "inst imm8 is 7");
}

static void test_zir_block_code_vector() {
    Arena arena;
    ZirBlock block(arena);
    CHECK_EQ(block.code.size(), 0u, "new block has empty code");

    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Push, 2);
    block.code.emplace(ZirOp::AddI);
    CHECK_EQ(block.code.size(), 3u, "block has 3 instructions");
    CHECK_EQ(block.code[0].op, ZirOp::Push, "first inst is Push");
    CHECK_EQ(block.code[1].op, ZirOp::Push, "second inst is Push");
    CHECK_EQ(block.code[2].op, ZirOp::AddI, "third inst is AddI");
}

static void test_zir_fn_fields() {
    Arena arena;
    ZirFn fn(arena);
    fn.name        = "main";
    fn.param_count = 2;
    fn.local_count = 4;
    fn.ret_type    = kVoidType;

    CHECK_EQ(fn.name, "main", "fn name is main");
    CHECK_EQ(fn.param_count, 2u, "fn has 2 params");
    CHECK_EQ(fn.local_count, 4u, "fn has 4 locals");
    CHECK_EQ(fn.ret_type, kVoidType, "fn returns void");
}

static void test_zir_fn_blocks() {
    Arena arena;
    ZirFn fn(arena);
    fn.name = "foo";

    auto &b1 = fn.blocks.emplace(arena);
    auto &b2 = fn.blocks.emplace(arena);
    CHECK_EQ(fn.blocks.size(), 2u, "fn has 2 blocks");

    b1.code.emplace(ZirOp::Ret);
    b2.code.emplace(ZirOp::Halt);
    CHECK_EQ(fn.blocks[0].code[0].op, ZirOp::Ret, "first block has Ret");
    CHECK_EQ(fn.blocks[1].code[0].op, ZirOp::Halt, "second block has Halt");
}

static void test_zir_module_functions() {
    Arena arena;
    ZirModule mod(arena);
    CHECK_EQ(mod.functions.size(), 0u, "new module has no functions");

    ZirFn fn1(arena);
    fn1.name = "main";
    mod.functions.push(std::move(fn1));

    ZirFn fn2(arena);
    fn2.name = "helper";
    mod.functions.push(std::move(fn2));

    CHECK_EQ(mod.functions.size(), 2u, "module has 2 functions");
    CHECK_EQ(mod.functions[0].name, "main", "first fn is main");
    CHECK_EQ(mod.functions[1].name, "helper", "second fn is helper");
}

static void test_zir_module_constants() {
    Arena arena;
    ZirModule mod(arena);
    CHECK_EQ(mod.constants.size(), 0u, "new module has no constants");

    mod.constants.push(int64_t{42});
    mod.constants.push(double{3.14});
    mod.constants.push(std::string("hello"));

    CHECK_EQ(mod.constants.size(), 3u, "module has 3 constants");

    CHECK(std::holds_alternative<int64_t>(mod.constants[0]), "first const is int64");
    CHECK_EQ(std::get<int64_t>(mod.constants[0]), 42, "int const is 42");

    CHECK(std::holds_alternative<double>(mod.constants[1]), "second const is double");
    CHECK_EQ(std::get<double>(mod.constants[1]), 3.14, "double const is 3.14");

    CHECK(std::holds_alternative<std::string>(mod.constants[2]), "third const is string");
    CHECK_EQ(std::get<std::string>(mod.constants[2]), "hello", "string const is hello");
}

static void test_zir_op_enum_values() {
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Halt), 0u, "Halt=0");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Push), 1u, "Push=1");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Dup), 2u, "Dup=2");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::AddI), 6u, "AddI=6");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::CmpI), 10u, "CmpI=10");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::AddF), 11u, "AddF=11");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Call), 19u, "Call=19");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Ret), 20u, "Ret=20");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Write), 21u, "Write=21");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Input), 22u, "Input=22");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::CastI32), 23u, "CastI32=23");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::CastF64), 24u, "CastF64=24");
    CHECK_EQ(static_cast<uint8_t>(ZirOp::Nop), 25u, "Nop=25");
}

static void test_zir_inst() {
    test_zir_inst_default_constructor();
    test_zir_inst_constructor();
    test_zir_block_code_vector();
    test_zir_fn_fields();
    test_zir_fn_blocks();
    test_zir_module_functions();
    test_zir_module_constants();
    test_zir_op_enum_values();
}

TEST_MAIN(zir_inst)
