#pragma once
#include "cli/options.hpp"
#include <vector>

namespace zith::cli::commands {

int check(const Options &opts);
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
int completion(const Options &opts);
int version();
int help(FILE *dest = stdout);

// Shared helpers
std::vector<std::string> collectFiles(const Options &opts);
std::vector<bool> runOnFiles(const Options &opts, const std::vector<std::string> &files,
                             session::Stage stage);
size_t countPassed(const std::vector<bool> &results);

} // namespace zith::cli::commands
