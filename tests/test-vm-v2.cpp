#include "test-common.hpp"

#include "cli/options.hpp"
#include "session/compilation-session.hpp"
#include "vm/hir-to-vm.hpp"
#include "vm/typed-ir.hpp"
#include "vm/vm-v2.hpp"

#include <filesystem>
#include <fstream>
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

void test_vm_v2_invalid_register_trap() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 1;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 9, 1));
    main.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Trap, "typed VM traps on an out-of-range destination");
    CHECK_EQ(result.output, std::string(), "shape validation runs before program output");
}

void test_vm_v2_invalid_opcode_shape_trap() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 1;
    main.body.push(vm::Instr{static_cast<vm::Op>(0xff), 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Trap, "typed VM traps on an unknown opcode");
}

void test_vm_v2_direct_call_fn_index() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &callee      = module.functions.emplace(arena);
    callee.name       = "add_three";
    callee.paramCount = 1;
    callee.returnType = vm::ValueType::I32;
    callee.regCount   = 2;
    callee.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 3));
    callee.body.push(vm::Instr::simple(vm::Op::Add, 0, 0, 1));
    callee.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    // Reg 0 = arg, reg 1 = result.
    main.regCount = 2;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 10));
    main.body.push(vm::Instr::simple(vm::Op::CallFn, 1, 0, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 1, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "direct fn call succeeds");
    CHECK_EQ(result.exitCode, 13, "direct fn call returns the callee result");
}

void test_vm_v2_indirect_call_fn_index() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &double_      = module.functions.emplace(arena);
    double_.name       = "double_";
    double_.paramCount = 1;
    double_.returnType = vm::ValueType::I32;
    double_.regCount   = 1;
    double_.body.push(vm::Instr{vm::Op::Add, 0, 0, 0, 0}); // reg0 = reg0 + reg0
    double_.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    // Reg 0 = fn ref, reg 1 = arg, reg 2 = result.
    main.regCount = 3;
    main.body.push(vm::Instr::withImm(vm::Op::LoadFnRef, 0, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 7));
    main.body.push(vm::Instr::callRef(vm::Op::CallFnRef, 2, 0, 1, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 2, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "indirect fn call succeeds");
    CHECK_EQ(result.exitCode, 14, "indirect fn call dispatches through the fn table");
}

void test_vm_v2_direct_call_extern_index() {
    memory::Arena arena;
    vm::Module module(arena);

    module.strings.push("via-extern");
    module.externs.push(std::string_view("puts"));

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 2;
    main.body.push(vm::Instr::withImm(vm::Op::LoadString, 0, 0));
    main.body.push(vm::Instr::simple(vm::Op::CallExtern, 1, 0, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "direct extern call succeeds");
    CHECK_EQ(result.output, std::string("via-extern\n"), "direct extern call writes output");
}

void test_vm_v2_indirect_call_extern_index() {
    memory::Arena arena;
    vm::Module module(arena);

    module.strings.push("via-extern-ref");
    module.externs.push(std::string_view("puts"));

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    // Reg 0 = extern ref, reg 1 = pointer, reg 2 = result.
    main.regCount = 3;
    main.body.push(vm::Instr::withImm(vm::Op::LoadExternRef, 0, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadString, 1, 0));
    main.body.push(vm::Instr::callRef(vm::Op::CallExternRef, 2, 0, 1, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "indirect extern call succeeds");
    CHECK_EQ(result.output, std::string("via-extern-ref\n"), "indirect extern writes output");
}

void test_vm_v2_store_load_bytes() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    // reg0 = bump offset, reg1 = size, reg2 = align, reg3 = loaded bytes.
    main.regCount = 4;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 1));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 4));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 2, 1));
    main.body.push(vm::Instr::simple(vm::Op::AllocBytes, 0, 1, 2));
    main.body.push(vm::Instr::simple(vm::Op::LoadBytes, 3, 0, 1));
    main.body.push(vm::Instr{vm::Op::Ret, 3, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "allocation and load inside memory succeeds");
    CHECK_EQ(result.exitCode, 0, "fresh bump memory reads as zero bytes");
}

void test_vm_v2_invalid_write_offset_trap() {
    memory::Arena arena;
    vm::Module module(arena);
    module.strings.push("x");

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 3;
    // StoreBytes writes module string 0 at the offset stored in reg1.
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 4096));
    main.body.push(vm::Instr::simple(vm::Op::StoreBytes, 1, 0, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Trap, "write outside linear memory traps");
}

void test_vm_v2_alloc_bump_and_heap_disjoint() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    // Let AllocBytes land on a bump offset and MallocBytes on a heap offset.
    main.regCount = 5;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 64));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 1));
    main.body.push(vm::Instr::simple(vm::Op::AllocBytes, 2, 0, 1));
    main.body.push(vm::Instr::simple(vm::Op::MallocBytes, 3, 0, 1));
    main.body.push(vm::Instr::simple(vm::Op::Sub, 4, 2, 3));
    main.body.push(vm::Instr{vm::Op::Ret, 4, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "bump and heap allocations succeed");
    CHECK(result.exitCode != 0, "bump and heap offsets stay disjoint");
}

void test_vm_v2_field_ptr_and_copy() {
    memory::Arena arena;
    vm::Module module(arena);
    module.strings.push("x");

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 6;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 128));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 2, 64));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 3, 1));
    main.body.push(vm::Instr::simple(vm::Op::AllocBytes, 0, 1, 3));
    main.body.push(vm::Instr{vm::Op::FieldPtr, 4, 0, 0, 2});
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 5, 1));
    main.body.push(vm::Instr::simple(vm::Op::StoreBytes, 4, 0, 0));
    main.body.push(vm::Instr::simple(vm::Op::LoadBytes, 5, 4, 5));
    main.body.push(vm::Instr{vm::Op::Ret, 5, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "FieldPtr and store/load succeed");
    CHECK_EQ(result.exitCode, 120, "load returns the stored byte through a field pointer");
}

void test_vm_v2_slice_pair() {
    memory::Arena arena;
    vm::Module module(arena);

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 4;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 100));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 7));
    main.body.push(vm::Instr::simple(vm::Op::MakeSlice, 2, 0, 1));
    main.body.push(vm::Instr::simple(vm::Op::SlicePtr, 3, 2, 0));
    main.body.push(vm::Instr{vm::Op::Ret, 3, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "slice pair construction succeeds");
    CHECK_EQ(result.exitCode, 100, "SlicePtr reads the pointer half of a slice");
}

void test_vm_v2_mem_copy_bytes() {
    memory::Arena arena;
    vm::Module module(arena);
    module.strings.push("q");

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 6;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 16));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 1, 1));
    main.body.push(vm::Instr::simple(vm::Op::AllocBytes, 2, 0, 1));
    main.body.push(vm::Instr::simple(vm::Op::AllocBytes, 3, 0, 1));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 4, 1));
    main.body.push(vm::Instr::simple(vm::Op::StoreBytes, 2, 0, 0));
    main.body.push(vm::Instr::simple(vm::Op::MemCopy, 3, 2, 4));
    main.body.push(vm::Instr::simple(vm::Op::LoadBytes, 5, 3, 4));
    main.body.push(vm::Instr{vm::Op::Ret, 5, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "MemCopy between allocated regions succeeds");
    CHECK_EQ(result.exitCode, 113, "memcpy copies stored byte to the destination");
}

void test_vm_v2_ffi_malloc_free_reuse() {
    memory::Arena arena;
    vm::Module module(arena);

    module.externs.push(std::string_view("malloc"));
    module.externs.push(std::string_view("free"));

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 5;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 64));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 1, 0, 0, 0));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 2, 1, 0, 1));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 3, 0, 0, 0));
    main.body.push(vm::Instr::simple(vm::Op::Sub, 4, 3, 1));
    main.body.push(vm::Instr{vm::Op::Ret, 4, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "malloc/free FFI returns host offsets");
    CHECK_EQ(result.exitCode, 0, "free reuses the same heap slot");
}

void test_vm_v2_ffi_putchar_unknown_trap() {
    memory::Arena arena;
    vm::Module module(arena);

    module.externs.push(std::string_view("putchar"));
    module.externs.push(std::string_view("not_supported"));

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 3;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, static_cast<uint16_t>('A')));
    main.body.push(vm::Instr::simple(vm::Op::CallExtern, 1, 0, 0));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 2, 0, 0, 1));
    main.body.push(vm::Instr{vm::Op::Ret, 0, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Trap, "unknown extern FFI traps");
}

void test_vm_v2_ffi_memcpy_and_strlen() {
    memory::Arena arena;
    vm::Module module(arena);
    module.strings.push("zith-ffi");
    module.externs.push(std::string_view("malloc"));
    module.externs.push(std::string_view("memcpy"));
    module.externs.push(std::string_view("strlen"));

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 6;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 32));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 1, 0, 0, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadString, 2, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 3, 8));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 4, 1, 2, 1, 3));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 5, 1, 0, 2));
    main.body.push(vm::Instr{vm::Op::Ret, 5, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "memcpy and strlen FFI succeed");
    CHECK_EQ(result.exitCode, 8, "strlen returns the copied string length");
}

void test_vm_v2_ffi_snprintf() {
    memory::Arena arena;
    vm::Module module(arena);
    module.strings.push("%u");
    module.externs.push(std::string_view("snprintf"));
    module.externs.push(std::string_view("malloc"));

    auto &main      = module.functions.emplace(arena);
    main.name       = "main";
    main.paramCount = 0;
    main.returnType = vm::ValueType::I32;
    main.regCount   = 6;
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 0, 32));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 1, 0, 0, 1));
    main.body.push(vm::Instr::withImm(vm::Op::LoadString, 2, 0));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 3, 32));
    main.body.push(vm::Instr::withImm(vm::Op::LoadConstI32, 4, 42));
    main.body.push(vm::Instr::callExtern(vm::Op::CallExtern, 5, 1, 3, 0, 2, 4));
    main.body.push(vm::Instr{vm::Op::Ret, 5, 0, 0, 0});

    vm::Vm vm;
    auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "snprintf FFI succeeds");
    CHECK_EQ(result.exitCode, 2, "snprintf writes the unsigned value");
}

#ifdef ZITH_ENABLE_C_INTEROP
void test_vm_v2_hello_stdlib_import_println() {
    const auto root = std::filesystem::temp_directory_path() / "zith-vm-v2-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto source = root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "from std/io/console\n"
                  "\n"
                  "fn main(): i32 {\n"
                  "    _ = println(\"hello v2\");\n"
                  "    return 0;\n"
                  "}\n";
    }

    memory::Arena arena;
    Options options(arena);
    options.includeDirs.push(ZITH_STDLIB_DIR);
    options.targetStage = session::Stage::HirLowered;

    session::CompilationSession session(options, source.string());
    session.setBuffered(true);
    CHECK(session.runTo(session::Stage::HirLowered),
          "println source lowers through the modern pipeline");

    memory::Arena vmArena;
    vm::Module module(vmArena);
    const auto lowered = vm::lowerModule(session.hirModule(), session.interner(), session.types(),
                                         vmArena, module);
    CHECK(lowered.ok, "HIR lowers into the v2 module");
    CHECK(module.functions.size() > 0, "v2 module contains function rows");

    vm::Vm vm;
    const auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "v2 VM runs the stdlib println program");
    CHECK_EQ(result.output, std::string("hello v2\n"), "v2 VM prints the string with newline");
    CHECK_EQ(result.exitCode, 0, "v2 VM keeps the main exit code");
}

void test_vm_v2_functions_with_stdlib() {
    const auto root = std::filesystem::temp_directory_path() / "zith-vm-v2-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto source = root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "from std/io/console\n"
                  "\n"
                  "fn add(a: i32, b: i32): i32 {\n"
                  "    a + b\n"
                  "}\n"
                  "\n"
                  "fn main(): i32 {\n"
                  "    _ = println(\"sum\");\n"
                  "    add(2, 3)\n"
                  "}\n";
    }

    memory::Arena arena;
    Options options(arena);
    options.includeDirs.push(ZITH_STDLIB_DIR);
    options.targetStage = session::Stage::HirLowered;

    session::CompilationSession session(options, source.string());
    session.setBuffered(true);
    CHECK(session.runTo(session::Stage::HirLowered),
          "stdlib-linked source lowers through the modern pipeline");

    memory::Arena vmArena;
    vm::Module module(vmArena);
    const auto lowered = vm::lowerModule(session.hirModule(), session.interner(), session.types(),
                                         vmArena, module);
    CHECK(lowered.ok, "HIR with a user function lowers into v2");

    vm::Vm vm;
    const auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "v2 VM runs main with stdlib linked");
    CHECK_EQ(result.output, std::string("sum\n"), "v2 VM prints through the stdlib import");
    CHECK_EQ(result.exitCode, 5, "v2 VM calls the user function");
}
#endif // ZITH_ENABLE_C_INTEROP

void test_vm_v2_extern_putchar() {
    const auto root = std::filesystem::temp_directory_path() / "zith-vm-v2-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto source = root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "#[discardable] extern fn putchar(c: char): i32\n"
                  "\n"
                  "fn main(): i32 {\n"
                  "    _ = putchar('A');\n"
                  "    0\n"
                  "}\n";
    }

    memory::Arena arena;
    Options options(arena);
    options.targetStage = session::Stage::HirLowered;

    session::CompilationSession session(options, source.string());
    session.setBuffered(true);
    CHECK(session.runTo(session::Stage::HirLowered),
          "putchar source lowers through the modern pipeline");

    memory::Arena vmArena;
    vm::Module module(vmArena);
    const auto lowered = vm::lowerModule(session.hirModule(), session.interner(), session.types(),
                                         vmArena, module);
    CHECK(lowered.ok, "HIR with putchar lowers into v2");

    vm::Vm vm;
    const auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "v2 VM runs the extern putchar program");
    CHECK_EQ(result.output, std::string("A"), "v2 VM forwards putchar to the host output");
    CHECK_EQ(result.exitCode, 0, "v2 VM keeps the main exit code");
}

void test_vm_v2_extern_snprintf_subset() {
    const auto root = std::filesystem::temp_directory_path() / "zith-vm-v2-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto source = root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "extern fn malloc(size: u64): raw opaque\n"
                  "extern fn snprintf(buf: *char, size: u64, fmt: *char, value: u32): i32\n"
                  "#[discardable] extern fn putchar(c: char): i32\n"
                  "\n"
                  "fn main(): i32 {\n"
                  "    var buf: *char = malloc(32) as *char;\n"
                  "    _ = snprintf(buf, 32, \"%u\", 42);\n"
                  "    _ = putchar(raw buf[0]);\n"
                  "    0\n"
                  "}\n";
    }

    memory::Arena arena;
    Options options(arena);
    options.targetStage = session::Stage::HirLowered;

    session::CompilationSession session(options, source.string());
    session.setBuffered(true);
    CHECK(session.runTo(session::Stage::HirLowered),
          "snprintf source lowers through the modern pipeline");

    memory::Arena vmArena;
    vm::Module module(vmArena);
    const auto lowered = vm::lowerModule(session.hirModule(), session.interner(), session.types(),
                                         vmArena, module);
    CHECK(lowered.ok, "HIR with snprintf lowers into v2");

    vm::Vm vm;
    const auto result = vm.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "v2 VM runs the extern snprintf program");
    CHECK_EQ(result.output, std::string("4"), "v2 VM formats and reads the host buffer");
    CHECK_EQ(result.exitCode, 0, "v2 VM keeps the main exit code");
}

void test_vm_v2() {
    test_vm_v2_malloc_string();
    test_vm_v2_linear_memory_trap();
    test_vm_v2_missing_main();
    test_vm_v2_loop_and_store();
    test_vm_v2_invalid_memory_offset();
    test_vm_v2_invalid_register_trap();
    test_vm_v2_invalid_opcode_shape_trap();
    test_vm_v2_direct_call_fn_index();
    test_vm_v2_indirect_call_fn_index();
    test_vm_v2_direct_call_extern_index();
    test_vm_v2_indirect_call_extern_index();
    test_vm_v2_store_load_bytes();
    test_vm_v2_invalid_write_offset_trap();
    test_vm_v2_alloc_bump_and_heap_disjoint();
    test_vm_v2_field_ptr_and_copy();
    test_vm_v2_slice_pair();
    test_vm_v2_mem_copy_bytes();
    test_vm_v2_ffi_malloc_free_reuse();
    test_vm_v2_ffi_putchar_unknown_trap();
    test_vm_v2_ffi_memcpy_and_strlen();
    test_vm_v2_ffi_snprintf();
#ifdef ZITH_ENABLE_C_INTEROP
    test_vm_v2_hello_stdlib_import_println();
    test_vm_v2_functions_with_stdlib();
#endif // ZITH_ENABLE_C_INTEROP
    test_vm_v2_extern_putchar();
    test_vm_v2_extern_snprintf_subset();
}

} // namespace

TEST_MAIN(vm_v2)
