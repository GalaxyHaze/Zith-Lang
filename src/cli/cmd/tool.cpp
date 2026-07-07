#include "cli/commands.hpp"
#include "cli/terminal.hpp"

#include <cstdio>

namespace zith::cli::commands {

namespace {

int stubCommand(const Options &opts, const char *name) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};
    err.yellow("[soon]");
    std::fprintf(stderr, " %s not implemented yet\n", name);
    return 1;
}

} // namespace

int test(const Options &opts) {
    return stubCommand(opts, "test");
}

int docs(const Options &opts) {
    return stubCommand(opts, "docs");
}

int repl(const Options &opts) {
    return stubCommand(opts, "repl");
}

int clean(const Options &opts) {
    return stubCommand(opts, "clean");
}

int deps(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};
    err.yellow("[soon]");
    std::fprintf(stderr, " deps not implemented yet\n");
    if (opts.subcommandStr.empty()) {
        std::fprintf(stderr,
                     "usage: zithc deps (list|add|remove|publish|unpublish|update) [args]\n");
    }
    return 1;
}

} // namespace zith::cli::commands
