#include "cli/commands.hpp"
#include "cli/terminal.hpp"

#include <cstdio>

namespace zith::cli::commands {

int repl(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};
    err.yellow("[soon]");
    std::fprintf(stderr, " not implemented yet\n");
    return 1;
}

} // namespace zith::cli::commands
