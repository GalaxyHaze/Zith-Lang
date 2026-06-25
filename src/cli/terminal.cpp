#include "cli/terminal.hpp"
#include "cli/options.hpp"

#ifdef _WIN32
#include <io.h>
#elif !defined(ZITH_IS_WASM)
#include <unistd.h>
#endif

namespace zith::cli::term {

static bool shouldUse(const std::string &setting, FILE *stream) {
    if (setting == "on")
        return true;
    if (setting == "off")
        return false;
#ifdef ZITH_IS_WASM
    (void)stream;
    return false;
#elif _WIN32
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
#ifdef ZITH_IS_WASM
    t.cerr_on = false;
    t.cout_on = false;
#elif _WIN32
    t.cerr_on = _isatty(_fileno(stderr));
    t.cout_on = _isatty(_fileno(stdout));
#else
    t.cerr_on = isatty(fileno(stderr));
    t.cout_on = isatty(fileno(stdout));
#endif
    return t;
}

bool useColor(const Options &opts) {
    return shouldUse(opts.color, stderr);
}

} // namespace zith::cli::term
