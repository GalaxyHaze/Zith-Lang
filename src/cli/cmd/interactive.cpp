#include "cli/commands.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>
#include <unistd.h>

namespace zith::cli::commands {

static bool useTermColor(const Options &opts, FILE *out) {
    if (opts.color == "on") return true;
    if (opts.color == "off") return false;
    return isatty(fileno(out)) != 0;
}
#define CERR(c) (useTermColor(opts, stderr) ? diagnostics::ansi::c.data() : "")
#define RERR   (useTermColor(opts, stderr) ? diagnostics::ansi::reset.data() : "")

int cmd_repl(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "%s[soon]%s not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

} // namespace zith::cli::commands
