#include "../../test-common.hpp"
#include "memory/arena.hpp"
#include "zir/zir/zir-inst.hpp"
#include "zir/zir/zir-interp.hpp"

#include <sstream>
#include <iostream>

using namespace zith::zir;
using zith::memory::Arena;

static void test_interpret_empty_module() {
    Arena arena;
    ZirModule mod(arena);
    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "empty module returns 0");
}

static void test_interpret_push_ret() {
    Arena arena;
    ZirModule mod(arena);
    ZirFn fn(arena);
    fn.name = "main";
    fn.param_count = 0;
    fn.local_count = 0;
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.constants.push(int64_t{42});
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "push+ret returns 0");
}

static void test_interpret_addi() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{10});
    mod.constants.push(int64_t{20});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::AddI);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "addi completes without crash");
}

static void test_interpret_subi() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{50});
    mod.constants.push(int64_t{15});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::SubI);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "subi completes without crash");
}

static void test_interpret_muli() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{6});
    mod.constants.push(int64_t{7});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::MulI);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "muli completes without crash");
}

static void test_interpret_divi() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{100});
    mod.constants.push(int64_t{4});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::DivI);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "divi completes without crash");
}

static void test_interpret_cmpi() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{3});
    mod.constants.push(int64_t{7});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::CmpI);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cmpi completes without crash");
}

static void test_interpret_dup() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{7});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Dup);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "dup completes without crash");
}

static void test_interpret_br_unconditional() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{1});
    mod.constants.push(int64_t{2});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Br, 2);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "br completes without crash");
}

static void test_interpret_brz_branch() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{0});
    mod.constants.push(int64_t{42});
    mod.constants.push(int64_t{99});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::BrZ, 2);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Push, 2);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brz branch completes without crash");
}

static void test_interpret_brz_no_branch() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{1});
    mod.constants.push(int64_t{42});
    mod.constants.push(int64_t{99});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::BrZ, 2);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Push, 2);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brz no-branch completes without crash");
}

static void test_interpret_store_load() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{123});

    ZirFn fn(arena);
    fn.name = "main";
    fn.local_count = 4;
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Store, 0, 0);
    block.code.emplace(ZirOp::Load, 0, 0);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "store/load completes without crash");
}

static void test_interpret_call() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{1});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Call, 0, 1);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "call completes without crash");
}

static void test_interpret_halt() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{1});
    mod.constants.push(int64_t{2});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Halt);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "halt stops early without crash");
}

static void test_interpret_write_stdout() {
    Arena arena;
    ZirModule mod(arena);

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Write);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    std::stringstream buffer;
    std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

    ZirInterpreter interp(mod);
    interp.run();

    std::cout.rdbuf(old);
    std::string output = buffer.str();
    CHECK_EQ(output, "Hello, World!\n", "write prints expected output");
}

static void test_interpret_addf() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(double{1.5});
    mod.constants.push(double{2.5});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::AddF);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "addf completes without crash");
}

static void test_interpret_subf() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(double{10.0});
    mod.constants.push(double{3.5});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::SubF);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "subf completes without crash");
}

static void test_interpret_mulf() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(double{3.0});
    mod.constants.push(double{4.0});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::MulF);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "mulf completes without crash");
}

static void test_interpret_divf() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(double{7.0});
    mod.constants.push(double{2.0});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::DivF);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "divf completes without crash");
}

static void test_interpret_cmpf() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(double{1.0});
    mod.constants.push(double{5.0});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::CmpF);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cmpf completes without crash");
}

static void test_interpret_brgt_branch() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{5});
    mod.constants.push(int64_t{42});
    mod.constants.push(int64_t{99});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::BrGt, 2);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Push, 2);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brgt branch completes without crash");
}

static void test_interpret_brgt_no_branch() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{0});
    mod.constants.push(int64_t{42});
    mod.constants.push(int64_t{99});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::BrGt, 2);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Push, 2);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brgt no-branch completes without crash");
}

static void test_interpret_pop() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{1});
    mod.constants.push(int64_t{2});
    mod.constants.push(int64_t{3});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::Push, 2);
    block.code.emplace(ZirOp::Pop);
    block.code.emplace(ZirOp::Pop);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "pop completes without crash");
}

static void test_interpret_cast_i32() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(double{3.14});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::CastI32);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cast i32 completes without crash");
}

static void test_interpret_cast_f64() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{5});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::CastF64);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cast f64 completes without crash");
}

static void test_interpret_nop() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{1});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Nop);
    block.code.emplace(ZirOp::Nop);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "nop completes without crash");
}

static void test_interpret_arithmetic_sequence() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{10});
    mod.constants.push(int64_t{3});
    mod.constants.push(int64_t{2});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Push, 1);
    block.code.emplace(ZirOp::AddI);
    block.code.emplace(ZirOp::Push, 2);
    block.code.emplace(ZirOp::MulI);
    block.code.emplace(ZirOp::Ret);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "arithmetic sequence completes without crash");
}

static void test_interpret_ret_with_value() {
    Arena arena;
    ZirModule mod(arena);
    mod.constants.push(int64_t{99});

    ZirFn fn(arena);
    fn.name = "main";
    ZirBlock block(arena);
    block.code.emplace(ZirOp::Push, 0);
    block.code.emplace(ZirOp::Ret, 0, 1);
    fn.blocks.push(std::move(block));
    mod.functions.push(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "ret with value completes without crash");
}

static void test_zir_interp() {
    test_interpret_empty_module();
    test_interpret_push_ret();
    test_interpret_addi();
    test_interpret_subi();
    test_interpret_muli();
    test_interpret_divi();
    test_interpret_cmpi();
    test_interpret_dup();
    test_interpret_br_unconditional();
    test_interpret_brz_branch();
    test_interpret_brz_no_branch();
    test_interpret_store_load();
    test_interpret_call();
    test_interpret_halt();
    test_interpret_write_stdout();
    test_interpret_addf();
    test_interpret_subf();
    test_interpret_mulf();
    test_interpret_divf();
    test_interpret_cmpf();
    test_interpret_brgt_branch();
    test_interpret_brgt_no_branch();
    test_interpret_pop();
    test_interpret_cast_i32();
    test_interpret_cast_f64();
    test_interpret_nop();
    test_interpret_arithmetic_sequence();
    test_interpret_ret_with_value();
}

TEST_MAIN(zir_interp)