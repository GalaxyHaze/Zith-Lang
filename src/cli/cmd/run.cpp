#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"

#include <cstdio>
#include <future>

namespace zith::cli::commands {

int execute(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter out{stdout, TERM.coutOn};
    term::UsagePrinter err{stderr, TERM.cerrOn};

    auto files = collectFiles(opts);
    if (files.empty()) {
        err.red("[error]");
        std::fprintf(stderr, " no input files and no ZithProject.toml found\n");
        return 1;
    }

    bool allPassed = true;
    int exitCode = 0;
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

        if (!session.linkAndExec()) {
            allPassed = false;
        } else {
            exitCode = session.childExitCode();
        }
    }

    if (opts.flags.verbose()) {
        if (allPassed)
            out.green("[ok]");
        else
            out.red("[error]");
        for (const auto &file : files)
            std::printf(" %s\n", file.c_str());
    }

    return allPassed ? exitCode : 1;
}

int run(const Options &opts) {
    return execute(opts);
}

} // namespace zith::cli::commands
