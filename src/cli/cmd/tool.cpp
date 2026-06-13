#include "cli/commands.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace zith::cli::commands {

static bool useTermColor(const Options &opts, FILE *out) {
    if (opts.color == "on") return true;
    if (opts.color == "off") return false;
#ifdef _WIN32
    return _isatty(_fileno(out)) != 0;
#else
    return isatty(fileno(out)) != 0;
#endif
}
#define CERR(c) (useTermColor(opts, stderr) ? diagnostics::ansi::c.data() : "")
#define RERR   (useTermColor(opts, stderr) ? diagnostics::ansi::reset.data() : "")

int cmd_test(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "%s[soon]%s not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

int cmd_fmt(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "%s[soon]%s not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

int cmd_docs(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "%s[soon]%s not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

} // namespace zith::cli::commands
