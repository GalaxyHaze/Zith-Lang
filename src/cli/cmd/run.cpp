#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "interp/hir-interpreter.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"
#ifdef ZITH_HAS_VM
#include "vm/hir-to-vm.hpp"
#include "vm/vm-v2.hpp"
#endif

#include <cstdio>
#include <string>

namespace zith::cli::commands {

namespace {

// The portable VM v2 path runs the program from HIR without native codegen.
// It is selected by --virtual-machine on any build, and automatically when the
// build has no LLVM codegen to link against. Returns false (and reports on
// stderr) when the program cannot be lowered or the VM traps.
bool runThroughVirtualMachine(session::CompilationSession &session, term::UsagePrinter &err,
                              int &exitCode) {
#ifdef ZITH_HAS_VM
    memory::Arena vmArena;
    vm::Module module(vmArena);
    const auto lowered = vm::lowerModule(session.hirModule(), session.interner(), session.types(),
                                         vmArena, module);
    if (!lowered.ok) {
        err.red("[error]");
        std::fprintf(stderr, " %s\n",
                     lowered.message.empty() ? "VM v2 could not lower the program"
                                             : lowered.message.c_str());
        return false;
    }

    vm::Vm machine;
    const auto result = machine.runMain(module);
    if (result.status != vm::RunStatus::Ok) {
        err.red("[error]");
        std::fprintf(stderr, " %s\n", result.message.empty() ? "VM v2 could not execute the program"
                                                             : result.message.c_str());
        return false;
    }
    std::fputs(result.output.c_str(), stdout);
    std::fflush(stdout);
    exitCode = static_cast<int>(result.exitCode);
    return true;
#else
    (void)session;
    (void)exitCode;
    err.red("[error]");
    std::fprintf(stderr,
                 " this build excludes the VM v2 slice; reconfigure with -DZITH_BUILD_VM=ON\n");
    return false;
#endif
}

// True when execution must go through the portable VM rather than native
// codegen: an explicit --virtual-machine, or a build without LLVM.
bool shouldUseVirtualMachine(const Options &opts) {
    if (opts.flags.virtualMachine())
        return true;
#if defined(ZITH_HAS_LLVM) && !defined(ZITH_IS_WASM)
    return false;
#else
    return true;
#endif
}

} // namespace

int execute(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};

    auto files = collectFiles(opts);
    if (files.empty()) {
        err.red("[error]");
        std::fprintf(stderr, " no input files and no ZithProject.toml found\n");
        return 1;
    }

    bool allPassed = true;
    int exitCode   = 0;
    for (const auto &file : files) {
        session::CompilationSession session(opts, file);
        session.setBuffered(true);
        session.setAlwaysEmitObject(true);
        bool ok = session.run();
        session.emitDiagnostics();
        std::fputs(session.flushOutput().c_str(), stderr);

        if (!ok) {
            allPassed = false;
            continue;
        }

        if (opts.flags.interpreted()) {
            interp::HirInterpreter interpreter(session.hirModule(), session.interner(),
                                               session.types());
            auto result = interpreter.runMain();
            if (result.status != interp::HirInterpStatus::Ok) {
                if (result.message.empty())
                    result.message = "HIR interpreter could not execute the program";
                err.red("[error]");
                std::fprintf(stderr, " %s\n", result.message.c_str());
                allPassed = false;
                continue;
            }
            std::fputs(result.output.c_str(), stdout);
            std::fflush(stdout);
            exitCode = static_cast<int>(result.exitCode);
            continue;
        }

        if (shouldUseVirtualMachine(opts)) {
            if (!runThroughVirtualMachine(session, err, exitCode)) {
                allPassed = false;
            }
            continue;
        }

        // The program inherits this process's stdout/stderr: its output is not
        // captured, so it stays interactive and unbuffered relative to the
        // terminal. Compiler logs remain in the session buffer -> stderr.
        std::fflush(stdout);
        bool executed = session.linkAndExecDirect();
        std::fputs(session.flushOutput().c_str(), stderr);

        if (!executed) {
            allPassed = false;
        } else {
            exitCode = session.childExitCode();
        }
    }

    if (opts.flags.verbose()) {
        // Compiler status stays on stderr so stdout carries only program output.
        if (allPassed)
            err.green("[ok]");
        else
            err.red("[error]");
        for (const auto &file : files)
            std::fprintf(stderr, " %s\n", file.c_str());
    }

    return allPassed ? exitCode : 1;
}

int run(const Options &opts) {
    return execute(opts);
}

} // namespace zith::cli::commands
