#include "cli/commands.hpp"
#include "cli/terminal.hpp"

#include <cstdio>
#include <string>

namespace zith::cli::commands {

int deps(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};

    err.yellow("[soon]");
    std::fprintf(stderr, " not implemented yet\n");

    if (opts.subcommandStr.empty()) {
        std::fprintf(stderr,
                     "usage: zithc deps (list|add|remove|publish|unpublish|update) [args]\n");
    }
    return 1;
}

} // namespace zith::cli::commands
