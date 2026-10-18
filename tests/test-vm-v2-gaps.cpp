#include "cli/options.hpp"
#include "session/compilation-session.hpp"
#include "test-common.hpp"
#include "vm/hir-to-vm.hpp"
#include "vm/vm-v2.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace zith;

namespace {

void test_vm_v2_gaps() {
    const auto root = std::filesystem::temp_directory_path() / "zith-vm-v2-gap-tests";
    std::filesystem::create_directories(root);
    const auto source = root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "fn main(): i32 { return 7; }\n";
        CHECK(output.good(), "gap smoke source is written");
    }

    memory::Arena arena;
    Options options(arena);
    options.targetStage = session::Stage::HirLowered;

    session::CompilationSession compilation(options, source.string());
    compilation.setBuffered(true);
    const bool hirReady = compilation.runTo(session::Stage::HirLowered);
    CHECK(hirReady, "pure Zith-- smoke source lowers through HIR");
    if (!hirReady)
        return;

    memory::Arena vmArena;
    vm::Module module(vmArena);
    const auto lowered = vm::lowerModule(compilation.hirModule(), compilation.interner(),
                                         compilation.types(), vmArena, module);
    CHECK(lowered.ok, "smoke source lowers into VM v2");
    if (!lowered.ok)
        return;

    vm::Vm machine;
    const auto result = machine.runMain(module);
    CHECK(result.status == vm::RunStatus::Ok, "VM v2 executes the smoke source");
    CHECK_EQ(result.exitCode, 7, "VM v2 returns the smoke source exit code");
}

} // namespace

TEST_MAIN(vm_v2_gaps)
