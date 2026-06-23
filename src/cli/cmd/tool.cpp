#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>

namespace zith::cli::commands {

#define CERR(c) term::err(TERM, diagnostics::ansi::c.data())
#define RERR   term::err_rst(TERM)

int cmd_test(const Options &opts) {
    auto TERM = term::init(opts);
    std::fprintf(stderr, "%s[soon]%s not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

int cmd_docs(const Options &opts) {
    auto TERM = term::init(opts);
    std::fprintf(stderr, "%s[soon]%s not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

} // namespace zith::cli::commands
