#include "cli/commands.hpp"

#include <cstdio>

namespace zith::cli::commands {

// TODO: implement interactive REPL loop
//       - read-eval-print loop with full pipeline per expression
//       - history, tab-completion, multi-line input
int cmd_repl(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[!] Command 'repl' is not implemented yet.\n");
    std::fprintf(stderr, "    This feature will be available in a future version of Zith.\n");
    return 1;
}

} // namespace zith::cli::commands
