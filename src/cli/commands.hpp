#pragma once
#include "cli/options.hpp"

namespace zith::cli::commands {

int check(const Options &opts);
int compile(const Options &opts);
int build(const Options &opts);
int execute(const Options &opts);
int run(const Options &opts);
int test(const Options &opts);
int fmt(const Options &opts);
int docs(const Options &opts);
int repl(const Options &opts);
int create(const Options &opts);
int clean(const Options &opts);
int deps(const Options &opts);
int version();
int help(FILE *dest = stdout);

} // namespace zith::cli::commands
