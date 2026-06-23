#include "cli/terminal.hpp"
#include "cli/options.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace zith::cli::term {

static bool shouldUse(const std::string &setting, FILE *stream) {
    if (setting == "on")
        return true;
    if (setting == "off")
        return false;
#ifdef _WIN32
    return _isatty(_fileno(stream)) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}

Term init(const Options &opts) {
    Term t;
    t.cerr_on = shouldUse(opts.color, stderr);
    t.cout_on = shouldUse(opts.color, stdout);
    return t;
}

Term init() {
    Term t;
    t.cerr_on = isatty(fileno(stderr));
    t.cout_on = isatty(fileno(stdout));
    return t;
}

bool useColor(const Options &opts) {
    return shouldUse(opts.color, stderr);
}

} // namespace zith::cli::term
