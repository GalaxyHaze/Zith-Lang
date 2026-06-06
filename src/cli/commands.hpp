#pragma once

#include "cli/options.hpp"

namespace zith::cli::commands {

int cmd_check(const Options &opts);
int cmd_compile(const Options &opts);
int cmd_build(const Options &opts);
int cmd_execute(const Options &opts);
int cmd_run(const Options &opts);
int cmd_test(const Options &opts);
int cmd_fmt(const Options &opts);
int cmd_docs(const Options &opts);
int cmd_repl(const Options &opts);
int cmd_new(const Options &opts);
int cmd_clean(const Options &opts);
int cmd_deps(const Options &opts);
int cmd_version();
int cmd_help();

} // namespace zith::cli::commands
