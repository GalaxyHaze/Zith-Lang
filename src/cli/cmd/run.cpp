#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "session/compilation-session.hpp"

#include <cstdio>

namespace zith::cli::commands {

int execute(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter out{stdout, TERM.coutOn};
    term::UsagePrinter err{stderr, TERM.cerrOn};

    if (opts.inputFiles.empty()) {
        err.red("[error]");
        std::fprintf(stderr, " no input files\n");
        return 1;
    }

    for (const auto &file : opts.inputFiles) {
        session::CompilationSession session(opts, file);
        bool ok = session.run();
        if (session.hasErrors())
            ok = false;
        if (opts.flags.verbose()) {
            ok ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", file.c_str());
        }
        if (!ok)
            return 1;
    }

    err.yellow("[soon]");
    std::fprintf(stderr, " execution not implemented yet\n");
    return 1;
}

int run(const Options &opts) {
    return execute(opts);
}

} // namespace zith::cli::commands
