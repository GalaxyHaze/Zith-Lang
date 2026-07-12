#include "cli/terminal.hpp"
#include "cli/options.hpp"

#ifdef _WIN32
#include <io.h>
#elif !defined(ZITH_IS_WASM)
#include <unistd.h>
#endif

namespace zith::term {

static bool shouldUse(const Options::Color color, FILE *stream) {
    if (color == Options::Color::On)
        return true;
    if (color == Options::Color::Off)
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
    t.cerrOn = shouldUse(opts.flags.color(), stderr);
    t.coutOn = shouldUse(opts.flags.color(), stdout);
    return t;
}

Term init() {
    Term t;
#ifdef ZITH_IS_WASM
    t.cerrOn = false;
    t.coutOn = false;
#elif _WIN32
    t.cerrOn = _isatty(_fileno(stderr));
    t.coutOn = _isatty(_fileno(stdout));
#else
    t.cerrOn = isatty(fileno(stderr));
    t.coutOn = isatty(fileno(stdout));
#endif
    return t;
}

bool useColor(const Options &opts) {
    return shouldUse(opts.flags.color(), stderr);
}

} // namespace zith::term
