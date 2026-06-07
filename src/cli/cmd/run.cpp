#include "cli/commands.hpp"
#include "cli/compilation-session.hpp"

#include <cstdio>

namespace zith::cli::commands {

int cmd_execute(const Options &opts) {
    if (opts.input_files.empty()) {
        std::fprintf(stderr, "no input files\n");
        return 1;
    }

    // TODO: execute = full pipeline + run interpreted binary
    // For now, run the pipeline to completion (ZirInterpreted)
    for (const auto &file : opts.input_files) {
        CompilationSession session(opts, file);
        bool ok = session.run();
        if (session.hasErrors())
            ok = false;
        if (opts.verbose)
            std::printf("[%s] %s\n", ok ? "ok" : "error", file.c_str());
        if (!ok)
            return 1;
    }

    // TODO: execute the compiled output / interpreted module
    std::fprintf(stderr, "[soon] execution not implemented yet\n");
    return 1;
}

int cmd_run(const Options &opts) {
    // run = compile + execute in one step
    return cmd_execute(opts);
}

} // namespace zith::cli::commands
