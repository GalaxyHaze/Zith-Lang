#include "cli/commands.hpp"
#include "cli/compilation-session.hpp"
#include "cli/terminal.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>

namespace zith::cli::commands {

#define CERR(c) term::err(TERM, diagnostics::ansi::c.data())
#define RERR   term::err_rst(TERM)
#define COUT(c) term::out(TERM, diagnostics::ansi::c.data())
#define ROUT   term::out_rst(TERM)

int cmd_execute(const Options &opts) {
    auto TERM = term::init(opts);
    if (opts.input_files.empty()) {
        std::fprintf(stderr, "%sno input files%s\n", CERR(red), RERR);
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
            std::printf("%s[%s]%s %s\n", ok ? COUT(green) : COUT(red), ok ? "ok" : "error", ROUT,
                        file.c_str());
        if (!ok)
            return 1;
    }

    // TODO: execute the compiled output / interpreted module
    std::fprintf(stderr, "%s[soon]%s execution not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

int cmd_run(const Options &opts) {
    // run = compile + execute in one step
    return cmd_execute(opts);
}

} // namespace zith::cli::commands
