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
    {"optional_none_is_null",
     "fn main(): i32 {\n"
     "    let value: ?i32 = null;\n"
     "    if (value is null) { return 17; }\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"optional_some_presence_and_must",
     "fn main(): i32 {\n"
     "    let value: ?i32 = 41;\n"
     "    if (value) { return must value; }\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"optional_pointer_niche",
     "fn main(): i32 {\n"
     "    var target: i32 = 23;\n"
     "    let absent: ?*i32 = null;\n"
     "    if (absent is null) {\n"
     "        let present: ?*i32 = &target;\n"
     "        return *must present;\n"
     "    }\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"implicit_numeric_enum",
     "enum Legacy { Base = -4, Next }\n"
     "fn main(): i32 {\n"
     "    let value: Legacy = Legacy.Next;\n"
     "    if (value as i32 == -3) { return 19; }\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"explicit_integer_enum",
     "enum ByteFlag: u8 { Off = 2, On = 255 }\n"
     "fn main(): i32 {\n"
     "    let value: ByteFlag = ByteFlag.On;\n"
     "    if (value as i32 == 255) { return 29; }\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"tagged_union_checks_and_narrowed_casts",
     "union Value { i32, bool }\n"
     "fn main(): i32 {\n"
     "    let number: Value = Value { 42 };\n"
     "    if (number is bool) { return 1; }\n"
     "    if (number is i32) {\n"
     "        let n: i32 = number;\n"
     "        let flag: Value = Value { true };\n"
     "        if (flag is bool) {\n"
     "            let yes: bool = flag;\n"
     "            if (yes) { return n; }\n"
     "        }\n"
     "    }\n"
     "    return 0;\n"
     "}\n",
     nullptr},
    {"raw_union_cast_in_raw_context",
     "raw union Bits { i32, i64 }\n"
     "fn main(): i32 {\n"
     "    let bits: Bits = Bits { 42 };\n"
     "    let value: i64 = raw bits as i64;\n"
     "    return value as i32;\n"
     "}\n",
     nullptr},
    // Passing slice: the VM and the native path agree on stdout and exit code.
    {"console_println_literal",
     "from std/io/console\n"
     "from std/memory\n"
     "\n"
     "fn main(): i32 {\n"
     "    let heap = HeapAllocator {};\n"
     "    _ = println(heap, \"hello parity\");\n"
     "    return 0;\n"
     "}\n",
     "dyn Allocator parameter layout is outside the VM v2 lowering subset "
     "(allocator ownership migration)"},
    {"console_print_no_newline",
     "from std/io/console\n"
     "from std/memory\n"
     "\n"
     "fn main(): i32 {\n"
     "    let heap = HeapAllocator {};\n"
     "    print(heap, \"a\");\n"
     "    print(heap, \"b\");\n"
     "    return 0;\n"
     "}\n",
     "dyn Allocator parameter layout is outside the VM v2 lowering subset "
     "(allocator ownership migration)"},
    {"console_println_two_literals",
     "from std/io/console\n"
     "from std/memory\n"
     "\n"
     "fn main(): i32 {\n"
     "    let heap = HeapAllocator {};\n"
     "    _ = println(heap, \"a\", \"b\");\n"
     "    return 0;\n"
     "}\n",
     "dyn Allocator parameter layout is outside the VM v2 lowering subset "
     "(allocator ownership migration)"},
    {"console_println_format_int",
     "from std/io/console\n"
     "from std/memory\n"
     "\n"
     "fn main(): i32 {\n"
     "    let heap = HeapAllocator {};\n"
     "    _ = println(heap, \"bucket=#\", 73);\n"
     "    return 0;\n"
     "}\n",
     "variadic Formatable rendering is outside the VM lowering subset (debt item 1)"},

    // Status-5 slice: the VM lowering does not yet cover these programs. The
    // native path runs them, so they are allow-listed with a reason and the
    // divergence is visible in the parity report.
    {"hash_map_u64_put_get",
     "import std/collections/hash_map_u64 as hm\n"
     "from std/memory\n"
     "\n"
     "fn main(): i32 {\n"
     "    let heap = HeapAllocator {};\n"
     "    var map = hm.create(heap);\n"
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
     "from std/memory\n"
     "\n"
     "fn main(): i32 {\n"
     "    let h = HeapAllocator {};\n"
     "    let mem = allocate(h, 64u64, 1u64);\n"
     "    if (mem is null) {\n"
     "        return 1;\n"
     "    }\n"
     "    deallocate(h, mem, 64u64, 1u64);\n"
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
    enum class VmPhase {
        NotStarted,
        HirLowering,
        VmLowering,
        VmExecution,
    };

    int status    = -1; // playground status: 0 ok, 3 trap, 4 oom, 5 unsupported
    int exitCode  = -1;
    VmPhase phase = VmPhase::NotStarted;
    std::string output;
    std::string detail;
};

const char *vmPhaseName(Observed::VmPhase phase) {
    switch (phase) {
    case Observed::VmPhase::NotStarted:
        return "not-started";
    case Observed::VmPhase::HirLowering:
        return "hir-lowering";
    case Observed::VmPhase::VmLowering:
        return "vm-lowering";
    case Observed::VmPhase::VmExecution:
        return "vm-execution";
    }
    return "unknown";
}

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
    observed.phase = Observed::VmPhase::HirLowering;
    if (!session.runTo(session::Stage::HirLowered)) {
        observed.status = kStatusUnsupported;
        for (const auto &diag : session.diags().all()) {
            if (diag.severity != diagnostics::Severity::Error)
                continue;
            if (observed.detail.empty()) {
                observed.detail = diag.message;
            } else {
                observed.detail += ": " + diag.message;
            }
        }
        if (observed.detail.empty())
            observed.detail = "HIR lowering failed without a diagnostic";
        return observed;
    }

    memory::Arena vmArena;
    vm::Module module(vmArena);
    observed.phase = Observed::VmPhase::VmLowering;
    const auto lowered =
        vm::lowerModule(session.hirModule(), session.interner(), session.types(), vmArena, module);
    if (!lowered.ok) {
        observed.status = kStatusUnsupported;
        observed.detail = lowered.message;
        if (observed.detail.empty())
            observed.detail = "VM lowering failed without a diagnostic";
        return observed;
    }

    observed.phase = Observed::VmPhase::VmExecution;
    vm::Vm machine;
    const auto result = machine.runMain(module);
    observed.status   = playgroundStatus(result.status);
    observed.exitCode = static_cast<int>(result.exitCode);
    observed.output   = result.output;
    observed.detail   = result.message;
    if (result.status != vm::RunStatus::Ok && observed.detail.empty())
        observed.detail =
            std::string("VM execution failed with status ") + playgroundStatusName(observed.status);
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
            const std::string message = label + " native path failed to run: " + native.detail +
                                        " (VM phase=" + vmPhaseName(vmRun.phase) +
                                        ", detail=" + vmRun.detail + ")";
            CHECK(false, message.c_str());
            continue;
        }

        if (sameBehavior(native, vmRun)) {
            CHECK(true, label.c_str());
            continue;
        }

        if (vmRun.status == kStatusUnsupported) {
            if (testCase.allowStatus5Reason != nullptr) {
                std::printf("  ALLOW: %s (status 5: %s; VM phase=%s, detail=%s)\n", testCase.name,
                            testCase.allowStatus5Reason, vmPhaseName(vmRun.phase),
                            vmRun.detail.c_str());
                CHECK(true, label.c_str());
            } else {
                const std::string message = label + " diverges with status 5 during " +
                                            vmPhaseName(vmRun.phase) +
                                            " and has no allow-list reason: " + vmRun.detail;
                CHECK(false, message.c_str());
            }
            continue;
        }

        // A divergence where the VM ran but produced different behavior is
        // never acceptable, even for allow-listed programs.
        std::printf("  native(exit=%d out=<%s>) vm(%s phase=%s exit=%d detail=<%s> out=<%s>)\n",
                    native.exitCode, native.output.c_str(), playgroundStatusName(vmRun.status),
                    vmPhaseName(vmRun.phase), vmRun.exitCode, vmRun.detail.c_str(),
                    vmRun.output.c_str());
        const std::string message = label + " diverges during " + vmPhaseName(vmRun.phase) + ": " +
                                    playgroundStatusName(vmRun.status) + " vs native";
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
