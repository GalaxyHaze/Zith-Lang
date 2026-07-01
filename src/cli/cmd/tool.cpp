#include "cli/commands.hpp"
#include "cli/terminal.hpp"

#include <cstdio>

namespace zith::cli::commands {

int test(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};
    err.yellow("[soon]");
    std::fprintf(stderr, " not implemented yet\n");
    return 1;
}

int docs(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};
    err.yellow("[soon]");
    std::fprintf(stderr, " not implemented yet\n");
    return 1;
}

} // namespace zith::cli::commands
