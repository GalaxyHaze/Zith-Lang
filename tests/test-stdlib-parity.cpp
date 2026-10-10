// Shared stdlib parity harness (VM v2 issue #72).
//
// The same stdlib-dependent programs run through the native `zithc` path and
// through the VM v2 path, and the observable behavior (stdout plus exit code)
// is compared. A program that the VM lowers to the unsupported playground
// status (5) while the native path succeeds is a documented divergence and is
// allowed only when the case carries an explicit reason in the table below.
// Any other divergence, including one that runs to completion with different
// output, fails the suite.
#include "test-common.hpp"

#if defined(ZITH_HAS_VM) && defined(ZITHC_BINARY) && defined(ZITH_ENABLE_C_INTEROP)

#include "cli/options.hpp"
#include "session/compilation-session.hpp"
#include "vm/hir-to-vm.hpp"
#include "vm/vm-v2.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace zith;

namespace {

namespace fs = std::filesystem;

// The playground ABI statuses mirror `src/wasm/playground.cpp`. The harness
// runs the VM directly, so it maps `vm::RunStatus` onto that scale to keep the
// "status 5" language in the issue meaningful.
constexpr int kStatusOk          = 0;
constexpr int kStatusTrap        = 3;
constexpr int kStatusOom         = 4;
constexpr int kStatusUnsupported = 5;

struct ParityCase {
    const char *name;
    const char *source;
    // Set for cases where the VM path legitimately reports status 5 while the
    // native path succeeds. The reason is required and is printed on failure.
    const char *allowStatus5Reason;
};

constexpr ParityCase kCases[] = {
    // Passing slice: the VM and the native path agree on stdout and exit code.
    {"console_println_literal",
     "from std/io/console\n"
     "\n"
     "fn main(): i32 {\n"
     "    _ = println(\"hello parity\");\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"console_print_no_newline",
     "from std/io/console\n"
     "\n"
     "fn main(): i32 {\n"
     "    print(\"a\");\n"
     "    print(\"b\");\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"console_println_two_literals",
     "from std/io/console\n"
     "\n"
     "fn main(): i32 {\n"
     "    _ = println(\"a\", \"b\");\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"console_println_format_int",
     "from std/io/console\n"
     "\n"
     "fn main(): i32 {\n"
     "    _ = println(\"bucket=#\", 73);\n"
     "    return 0;\n"
     "}\n",
     "variadic Formatable rendering is outside the VM lowering subset (debt item 1)"},

    // Status-5 slice: the VM lowering does not yet cover these programs. The
    // native path runs them, so they are allow-listed with a reason and the
    // divergence is visible in the parity report.
    {"hash_map_u64_put_get",
     "import std/collections/hash_map_u64 as hm\n"
     "\n"
     "fn main(): i32 {\n"
     "    var map = hm.HashMap { count: 0u64, capacity: 0u64, table: null, head: 0u64 };\n"
     "    if not(hm.HashMap.reserve(lend map, 4u64)) {\n"
     "        return 1;\n"
     "    }\n"
     "    if not(hm.HashMap.put(lend map, 1u64, 42u64)) {\n"
     "        return 2;\n"
     "    }\n"
     "    if not(hm.HashMap.contains(view map, 1u64)) {\n"
     "        return 3;\n"
     "    }\n"
     "    let value = hm.HashMap.get(view map, 1u64);\n"
     "    if (value is null or raw value != 42u64) {\n"
     "        return 5;\n"
     "    }\n"
     "    hm.HashMap.destroy(lend map);\n"
     "    return 0;\n"
     "}\n",
     "hash-map control flow hits a HIR branch shape the VM lowering does not handle "
     "(debt item 1)"},
    {"allocator_heap_dyn_dispatch",
     "import std/alloc\n"
     "\n"
     "fn main(): i32 {\n"
     "    let h = std.alloc.HeapAllocator {};\n"
     "    let mem = std.alloc.allocate(h, 64u64, 1u64);\n"
     "    if (mem is null) {\n"
     "        return 1;\n"
     "    }\n"
     "    std.alloc.deallocate(h, mem, 64u64, 1u64);\n"
     "    return 42;\n"
     "}\n",
     "dyn allocator dispatch is outside the VM lowering subset (debt items 1 and 4)"},
    {"inplace_opaque_contract",
     "from std/memory\n"
     "\n"
     "struct Box {\n"
     "    value: i64,\n"
     "}\n"
     "\n"
     "implement Box as InPlace {\n"
     "    fn inplace(var self, args: opaque): bool {\n"
     "        return true;\n"
     "    }\n"
     "    fn clean(var self) {}\n"
     "}\n"
     "\n"
     "fn main(): i32 {\n"
     "    var box = Box { value: 42 };\n"
     "    if not box.inplace(0) {\n"
     "        return 1;\n"
     "    }\n"
     "    return 42;\n"
     "}\n",
     "the opaque InPlace contract is outside the VM lowering subset (debt item 1)"},
};

std::string readFile(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

struct Observed {
    int status   = -1; // playground status: 0 ok, 3 trap, 4 oom, 5 unsupported
    int exitCode = -1;
    std::string output;
    std::string detail;
};

int playgroundStatus(vm::RunStatus status) {
    switch (status) {
    case vm::RunStatus::Ok:
        return kStatusOk;
    case vm::RunStatus::MissingMain:
    case vm::RunStatus::Trap:
        return kStatusTrap;
    case vm::RunStatus::Unsupported:
        return kStatusUnsupported;
    case vm::RunStatus::Oom:
        return kStatusOom;
    }
    return kStatusTrap;
}

const char *playgroundStatusName(int status) {
    switch (status) {
    case kStatusOk:
        return "ok";
    case kStatusTrap:
        return "trap";
    case kStatusOom:
        return "oom";
    case kStatusUnsupported:
        return "status-5";
    }
    return "unknown";
}

/// `zithc` writes `target/` and `cache/` next to the source, so each program
/// runs from its own work directory to keep the tree clean.
Observed runNative(const fs::path &workdir, const fs::path &source) {
    std::error_code ec;
    fs::remove_all(workdir / "cache", ec);
    fs::remove_all(workdir / "target", ec);
    fs::remove_all(workdir / ".zith-cache", ec);

    const fs::path stdoutPath = workdir / "native.stdout";
    const fs::path stderrPath = workdir / "native.stderr";
    const std::string command = std::string("cd \"") + workdir.string() + "\" && \"" +
                                ZITHC_BINARY + "\" --include \"" + ZITH_STDLIB_DIR + "\" run \"" +
                                source.string() + "\" > \"" + stdoutPath.string() + "\" 2> \"" +
                                stderrPath.string() + "\"";
    const int status          = std::system(command.c_str());

    Observed observed;
    if (status < 0) {
        observed.detail = "std::system failed to launch zithc";
        return observed;
    }
    if ((status & 0x7F) != 0) {
        observed.status   = -1;
        observed.exitCode = -1;
        observed.detail   = "zithc terminated by signal";
        return observed;
    }
    observed.status   = kStatusOk;
    observed.exitCode = (status >> 8) & 0xFF;
    observed.output   = readFile(stdoutPath);
    return observed;
}

Observed runVm(const fs::path &source) {
    memory::Arena arena;
    Options options(arena);
    options.includeDirs.push(ZITH_STDLIB_DIR);
    options.targetStage = session::Stage::HirLowered;

    session::CompilationSession session(options, source.string());
    session.setBuffered(true);
    Observed observed;
    if (!session.runTo(session::Stage::HirLowered)) {
        observed.status = kStatusUnsupported;
        observed.detail = "HIR lowering failed";
        return observed;
    }

    memory::Arena vmArena;
    vm::Module module(vmArena);
    const auto lowered =
        vm::lowerModule(session.hirModule(), session.interner(), session.types(), vmArena, module);
    if (!lowered.ok) {
        observed.status = kStatusUnsupported;
        observed.detail = lowered.message;
        return observed;
    }

    vm::Vm machine;
    const auto result = machine.runMain(module);
    observed.status   = playgroundStatus(result.status);
    observed.exitCode = static_cast<int>(result.exitCode);
    observed.output   = result.output;
    observed.detail   = result.message;
    return observed;
}

bool sameBehavior(const Observed &native, const Observed &vmRun) {
    return native.exitCode == vmRun.exitCode && native.output == vmRun.output;
}

} // namespace

void test_stdlib_parity() {
    const fs::path root = fs::temp_directory_path() / "zith-stdlib-parity-tests";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    for (const ParityCase &testCase : kCases) {
        const fs::path workdir = root / testCase.name;
        fs::create_directories(workdir, ec);
        const fs::path source = workdir / "main.zith";
        {
            std::ofstream out(source, std::ios::binary | std::ios::trunc);
            out << testCase.source;
        }

        const Observed native   = runNative(workdir, source);
        const Observed vmRun    = runVm(source);
        const std::string label = std::string("parity: ") + testCase.name;

        if (native.status != kStatusOk) {
            const std::string message = label + " native path failed to run: " + native.detail;
            CHECK(false, message.c_str());
            continue;
        }

        if (sameBehavior(native, vmRun)) {
            CHECK(true, label.c_str());
            continue;
        }

        if (vmRun.status == kStatusUnsupported) {
            if (testCase.allowStatus5Reason != nullptr) {
                std::printf("  ALLOW: %s (status 5: %s)\n", testCase.name,
                            testCase.allowStatus5Reason);
                CHECK(true, label.c_str());
            } else {
                const std::string message =
                    label + " diverges with status 5 and no allow-list reason: " + vmRun.detail;
                CHECK(false, message.c_str());
            }
            continue;
        }

        // A divergence where the VM ran but produced different behavior is
        // never acceptable, even for allow-listed programs.
        std::printf("  native(exit=%d out=<%s>) vm(%s exit=%d out=<%s>)\n", native.exitCode,
                    native.output.c_str(), playgroundStatusName(vmRun.status), vmRun.exitCode,
                    vmRun.output.c_str());
        const std::string message =
            label + " diverges: " + playgroundStatusName(vmRun.status) + " vs native";
        CHECK(false, message.c_str());
    }
}

int main() {
    std::printf("stdlib_parity tests\n");
    std::printf("=====================\n\n");
    g_test_passed = 0;
    g_test_failed = 0;
    test_stdlib_parity();
    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}

#else

int main() {
    std::printf(
        "test-stdlib-parity skipped: needs the VM v2 slice, a zithc target and C interop\n");
    return 77;
}

#endif
