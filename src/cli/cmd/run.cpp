#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "interp/hir-interpreter.hpp"
#include "interp/ir-vm.hpp"
#include "ir/hir-to-ir.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"

#include <cstdio>
#include <future>
#include <string>

namespace zith::cli::commands {

int execute(const Options &opts) {
#if defined(ZITH_HAS_LLVM) && !defined(ZITH_IS_WASM)
    const bool useIrVm = false;
#else
    const bool useIrVm = true;
#endif

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

        if (useIrVm) {
            memory::Arena irArena;
            ir::Module module(irArena);
            const auto lowered = ir::lowerModule(session.hirModule(), session.interner(),
                                                 session.types(), irArena, module);
            if (!lowered.ok) {
                err.red("[error]");
                std::fprintf(stderr, " %s\n", lowered.message.c_str());
                allPassed = false;
                continue;
            }

            interp::IrVm vm(irArena);
            const auto result = vm.runMain(module);
            if (result.status != interp::IrVmStatus::Ok) {
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
