#include "cli/commands.hpp"

#include <cstdio>
#include <string>

namespace zith::cli::commands {

namespace {

int depsList(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int depsAdd(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int depsRemove(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int depsPublish(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int depsUnpublish(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

int depsUpdate(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[soon] not implemented yet\n");
    return 1;
}

} // namespace

int cmd_deps(const Options &opts) {
    const std::string &sub = opts.subcommand_arg;

    if (sub == "list")
        return depsList(opts);
    if (sub == "add")
        return depsAdd(opts);
    if (sub == "remove")
        return depsRemove(opts);
    if (sub == "publish")
        return depsPublish(opts);
    if (sub == "unpublish")
        return depsUnpublish(opts);
    if (sub == "update")
        return depsUpdate(opts);

    std::fprintf(stderr, "[soon] not implemented yet\n"
                         "usage: zithc deps (list|add|remove|publish|unpublish|update) [args]\n");
    return 1;
}

} // namespace zith::cli::commands
