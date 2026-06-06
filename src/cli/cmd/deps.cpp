#include "cli/commands.hpp"

#include <cstdio>
#include <string>

namespace zith::cli::commands {

namespace {

int deps_list(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int deps_add(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int deps_remove(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int deps_publish(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int deps_unpublish(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int deps_update(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

} // namespace

int cmd_deps(const Options &opts) {
    const std::string &sub = opts.subcommand_arg;

    if (sub == "list")    return deps_list(opts);
    if (sub == "add")     return deps_add(opts);
    if (sub == "remove")  return deps_remove(opts);
    if (sub == "publish")  return deps_publish(opts);
    if (sub == "unpublish") return deps_unpublish(opts);
    if (sub == "update")  return deps_update(opts);

    std::fprintf(stderr,
        "[soon] not implemented yet\n"
        "usage: zith deps (list|add|remove|publish|unpublish|update) [args]\n");
    return 1;
}

} // namespace zith::cli::commands
