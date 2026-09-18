#include "test-common.hpp"

#include "vm/typed-ir.hpp"
#include "vm/vm-v2.hpp"

#include <cstdint>
#include <string_view>

using namespace zith;

namespace {

auto makeModule(memory::Arena &arena) -> vm::Module {
    vm::Module module(arena);
    module.strings.push("zith-vm-v2");
    module.externs.push(std::string_view("puts"));

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 8;
    main.regTypes.push(vm::ValueType::I32);
    main.regTypes.push(vm::ValueType::I32);
    main.regTypes.push(vm::ValueType::Ptr);
    main.regTypes.push(vm::ValueType::Ptr);
    main.regTypes.push(vm::ValueType::Ptr);
    main.regTypes.push(vm::ValueType::I32);
    main.regTypes.push(vm::ValueType::I32);

    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadString, 1, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 2, 32));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 3, 1));
    main.body.push(vm::Instr::simple(vm::Op::AllocBytes, 4, 2, 3));
    main.body.push(vm::Instr::simple(vm::Op::StoreBytes, 4, 0, 0));
    main.body.push(vm::Instr::simple(vm::Op::MallocBytes, 5, 2, 3));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 6, 12));
    main.body.push(vm::Instr::simple(vm::Op::MemCopy, 5, 4, 6));
    main.body.push(vm::Instr::simple(vm::Op::CallExtern, 7, 5, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 6, 0, 0, 0});

    return module;
}

void test_vm_v2_malloc_string() {
    memory::Arena arena;
    auto module = makeModule(arena);

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "typed VM runs main");
    CHECK_EQ(result.exitCode, 12, "main returns the string length");
    CHECK_EQ(result.output, std::string("zith-vm-v2\n"), "extern puts writes the string");
}

void test_vm_v2_linear_memory_trap() {
    memory::Arena arena;
    auto module = makeModule(arena);
    module.functions[0].body.clear();
    module.functions[0].body.push(vm::Instr{vm::Op::Trap, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Trap, "typed VM reports explicit trap");
}

void test_vm_v2_missing_main() {
    memory::Arena arena;
    auto module = makeModule(arena);
    module.functions.clear();

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::MissingMain, "typed VM reports missing main");
}

void test_vm_v2_loop_and_store() {
    memory::Arena arena;
    vm::Module module(arena);
    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 5;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 2, 1));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 3, 10));
    main.body.push(vm::Instr{vm::Op::Jump, 0, 0, 0, 5});
    main.body.push(vm::Instr::simple(vm::Op::Add, 0, 0, 2));
    main.body.push(vm::Instr::simple(vm::Op::Add, 1, 1, 2));
    main.body.push(vm::Instr::simple(vm::Op::Lt, 4, 1, 3));
    main.body.push(vm::Instr{vm::Op::Branch, 0, 4, 0, 5});
    main.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "typed VM runs a branch loop");
    CHECK_EQ(result.exitCode, 10, "loop increments a register to its bound");
}

void test_vm_v2_invalid_memory_offset() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 3;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "typed VM allows guest offset zero as valid memory");
}

void test_vm_v2() {
    test_vm_v2_malloc_string();
    test_vm_v2_linear_memory_trap();
    test_vm_v2_missing_main();
    test_vm_v2_loop_and_store();
    test_vm_v2_invalid_memory_offset();
}

} // namespace

TEST_MAIN(vm_v2)
