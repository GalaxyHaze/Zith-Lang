#include "cli/compiler-driver.hpp"
#include "cli/options.hpp"
#include "cli/project-config.hpp"
#include "support/platform.hpp"

#include <filesystem>

int main(const int argc, char **argv) {
    zith::support::enableVirtualTerminal();
    auto opts = zith::cli::parseArgs(argc, argv);

    // Search for ZithProject.toml from CWD upward and merge build settings.
    // CLI flags take priority over project config.
    namespace fs    = std::filesystem;
    fs::path search = fs::current_path();
    while (true) {
        auto toml_path = search / "ZithProject.toml";
        if (fs::exists(toml_path)) {
            if (auto cfg = zith::cli::ProjectConfig::load(toml_path.string()))
                zith::cli::mergeProjectConfig(opts, *cfg);
            break;
        }
        if (search == search.root_path())
            break;
        search = search.parent_path();
    }

    if (opts.verbose) {
        std::fprintf(stderr, "[config] Mode: %s | Opt: %d | Debug: %d\n", opts.mode.c_str(),
                     opts.opt_level, opts.debug_level);
        if (!opts.target_triple.empty())
            std::fprintf(stderr, "[config] Target: %s\n", opts.target_triple.c_str());
        if (!opts.include_dirs.empty()) {
            std::fprintf(stderr, "[config] Include dirs:");
            for (const auto &d : opts.include_dirs)
                std::fprintf(stderr, " %s", d.c_str());
            std::fprintf(stderr, "\n");
        }
    }

    return zith::cli::CompilerDriver::run(opts);
}
