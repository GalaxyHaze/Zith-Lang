#pragma once

#include <cstdio>

namespace zith::cli {

struct Options;

namespace term {

struct Term {
    bool cerr_on = false;
    bool cout_on = false;
};

Term init(const Options &opts);
Term init(); // auto-detect (for commands without Options)
bool useColor(const Options &opts); // quick check: should stderr use color?

// stderr helpers
inline const char *err(const Term &t, const char *code) { return t.cerr_on ? code : ""; }
inline const char *err_rst(const Term &t)               { return t.cerr_on ? "\033[0m" : ""; }

// stdout helpers
inline const char *out(const Term &t, const char *code) { return t.cout_on ? code : ""; }
inline const char *out_rst(const Term &t)               { return t.cout_on ? "\033[0m" : ""; }

} // namespace term
} // namespace zith::cli
