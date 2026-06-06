#include "cli/commands.hpp"

#include <cstdio>

namespace zith::cli::commands {

int cmd_check(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int cmd_compile(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int cmd_build(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

} // namespace zith::cli::commands
