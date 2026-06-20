#include "../../test-common.hpp"
#include "zir/zir/zir-inst.hpp"
#include "zir/zir/zir-interp.hpp"

#include <sstream>
#include <iostream>

using namespace zith::zir;

static void test_interpret_empty_module() {
    ZirModule mod;
    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "empty module returns 0");
}

static void test_interpret_push_ret() {
    ZirModule mod;
    ZirFn fn;
    fn.name = "main";
    fn.param_count = 0;
    fn.local_count = 0;
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.constants.emplace_back(int64_t{42});
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "push+ret returns 0");
}

static void test_interpret_addi() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{10});
    mod.constants.emplace_back(int64_t{20});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::AddI);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "addi completes without crash");
}

static void test_interpret_subi() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{50});
    mod.constants.emplace_back(int64_t{15});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::SubI);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "subi completes without crash");
}

static void test_interpret_muli() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{6});
    mod.constants.emplace_back(int64_t{7});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::MulI);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "muli completes without crash");
}

static void test_interpret_divi() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{100});
    mod.constants.emplace_back(int64_t{4});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::DivI);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "divi completes without crash");
}

static void test_interpret_cmpi() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{3});
    mod.constants.emplace_back(int64_t{7});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::CmpI);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cmpi completes without crash");
}

static void test_interpret_dup() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{7});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Dup);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "dup completes without crash");
}

static void test_interpret_br_unconditional() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{1});
    mod.constants.emplace_back(int64_t{2});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Br, 2);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "br completes without crash");
}

static void test_interpret_brz_branch() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{0});
    mod.constants.emplace_back(int64_t{42});
    mod.constants.emplace_back(int64_t{99});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::BrZ, 2);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::Push, 2);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brz branch completes without crash");
}

static void test_interpret_brz_no_branch() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{1});
    mod.constants.emplace_back(int64_t{42});
    mod.constants.emplace_back(int64_t{99});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::BrZ, 2);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::Push, 2);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brz no-branch completes without crash");
}

static void test_interpret_store_load() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{123});

    ZirFn fn;
    fn.name = "main";
    fn.local_count = 4;
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Store, 0, 0);
    block.code.emplace_back(ZirOp::Load, 0, 0);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "store/load completes without crash");
}

static void test_interpret_call() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{1});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Call, 0, 1);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "call completes without crash");
}

static void test_interpret_halt() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{1});
    mod.constants.emplace_back(int64_t{2});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Halt);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "halt stops early without crash");
}

static void test_interpret_write_stdout() {
    ZirModule mod;

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Write);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    std::stringstream buffer;
    std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());

    ZirInterpreter interp(mod);
    interp.run();

    std::cout.rdbuf(old);
    std::string output = buffer.str();
    CHECK_EQ(output, "Hello, World!\n", "write prints expected output");
}

static void test_interpret_addf() {
    ZirModule mod;
    mod.constants.emplace_back(double{1.5});
    mod.constants.emplace_back(double{2.5});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::AddF);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "addf completes without crash");
}

static void test_interpret_subf() {
    ZirModule mod;
    mod.constants.emplace_back(double{10.0});
    mod.constants.emplace_back(double{3.5});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::SubF);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "subf completes without crash");
}

static void test_interpret_mulf() {
    ZirModule mod;
    mod.constants.emplace_back(double{3.0});
    mod.constants.emplace_back(double{4.0});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::MulF);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "mulf completes without crash");
}

static void test_interpret_divf() {
    ZirModule mod;
    mod.constants.emplace_back(double{7.0});
    mod.constants.emplace_back(double{2.0});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::DivF);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "divf completes without crash");
}

static void test_interpret_cmpf() {
    ZirModule mod;
    mod.constants.emplace_back(double{1.0});
    mod.constants.emplace_back(double{5.0});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::CmpF);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cmpf completes without crash");
}

static void test_interpret_brgt_branch() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{5});
    mod.constants.emplace_back(int64_t{42});
    mod.constants.emplace_back(int64_t{99});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::BrGt, 2);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::Push, 2);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brgt branch completes without crash");
}

static void test_interpret_brgt_no_branch() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{0});
    mod.constants.emplace_back(int64_t{42});
    mod.constants.emplace_back(int64_t{99});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::BrGt, 2);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::Push, 2);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "brgt no-branch completes without crash");
}

static void test_interpret_pop() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{1});
    mod.constants.emplace_back(int64_t{2});
    mod.constants.emplace_back(int64_t{3});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::Push, 2);
    block.code.emplace_back(ZirOp::Pop);
    block.code.emplace_back(ZirOp::Pop);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "pop completes without crash");
}

static void test_interpret_cast_i32() {
    ZirModule mod;
    mod.constants.emplace_back(double{3.14});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::CastI32);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cast i32 completes without crash");
}

static void test_interpret_cast_f64() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{5});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::CastF64);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "cast f64 completes without crash");
}

static void test_interpret_nop() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{1});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Nop);
    block.code.emplace_back(ZirOp::Nop);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "nop completes without crash");
}

static void test_interpret_arithmetic_sequence() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{10});
    mod.constants.emplace_back(int64_t{3});
    mod.constants.emplace_back(int64_t{2});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Push, 1);
    block.code.emplace_back(ZirOp::AddI);
    block.code.emplace_back(ZirOp::Push, 2);
    block.code.emplace_back(ZirOp::MulI);
    block.code.emplace_back(ZirOp::Ret);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

    ZirInterpreter interp(mod);
    int result = interp.run();
    CHECK_EQ(result, 0, "arithmetic sequence completes without crash");
}

static void test_interpret_ret_with_value() {
    ZirModule mod;
    mod.constants.emplace_back(int64_t{99});

    ZirFn fn;
    fn.name = "main";
    ZirBlock block;
    block.code.emplace_back(ZirOp::Push, 0);
    block.code.emplace_back(ZirOp::Ret, 0, 1);
    fn.blocks.push_back(std::move(block));
    mod.functions.push_back(std::move(fn));

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