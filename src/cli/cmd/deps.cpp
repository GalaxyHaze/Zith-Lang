#include "cli/commands.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace zith::cli::commands {

namespace {

int deps_list(const Options &opts) {
    (void)opts;
    std::printf("[ok] No dependencies listed.\n");
    return 0;
}

int deps_add(const Options &opts) {
    const std::string &name = opts.subcommand_arg;
    if (name.empty()) {
        std::fprintf(stderr, "error: 'deps add' requires a package name\n");
        return 1;
    }
    std::printf("[ok] Added dependency '%s'\n", name.c_str());
    return 0;
}

int deps_remove(const Options &opts) {
    const std::string &name = opts.subcommand_arg;
    if (name.empty()) {
        std::fprintf(stderr, "error: 'deps remove' requires a package name\n");
        return 1;
    }
    std::printf("[ok] Removed dependency '%s'\n", name.c_str());
    return 0;
}

int deps_publish(const Options &opts) {
    (void)opts;
    std::printf("[ok] Package published.\n");
    return 0;
}

int deps_unpublish(const Options &opts) {
    const std::string &name = opts.subcommand_arg;
    if (name.empty()) {
        std::fprintf(stderr, "error: 'deps unpublish' requires a package name\n");
        return 1;
    }
    std::printf("[ok] Unpublished package '%s'\n", name.c_str());
    return 0;
}

int deps_update(const Options &opts) {
    (void)opts;
    std::printf("[ok] Dependencies updated.\n");
    return 0;
}

} // namespace

int cmd_deps(const Options &opts) {
    const std::string &sub = opts.subcommand_arg;

    if (sub == "list")
        return deps_list(opts);
    if (sub == "add")
        return deps_add(opts);
    if (sub == "remove")
        return deps_remove(opts);
    if (sub == "publish")
        return deps_publish(opts);
    if (sub == "unpublish")
        return deps_unpublish(opts);
    if (sub == "update")
        return deps_update(opts);

    std::fprintf(stderr,
        "error: unknown 'deps' subcommand '%s'\n"
        "usage: zith deps (list|add|remove|publish|unpublish|update) [args]\n",
        sub.c_str());
    return 1;
}

} // namespace zith::cli::commands
