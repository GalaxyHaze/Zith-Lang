#include "cli/compiler-driver.hpp"
#include "cli/options.hpp"
#include "cli/zith-flags.hpp"

#include <cstdio>
#include <cstdlib>

static void mergeFlags(zith::cli::Options &opts, const zith::cli::Options &defaults) {
    if (opts.output_file == "a.out" && defaults.output_file != "a.out")
        opts.output_file = defaults.output_file;
    if (opts.mode == "debug" && defaults.mode != "debug")
        opts.mode = defaults.mode;
    if (opts.target_triple.empty() && !defaults.target_triple.empty())
        opts.target_triple = defaults.target_triple;
    if (opts.opt_level == 0 && defaults.opt_level != 0)
        opts.opt_level = defaults.opt_level;
    if (opts.debug_level == 2 && defaults.debug_level != 2)
        opts.debug_level = defaults.debug_level;
    if (!opts.strict && defaults.strict)
        opts.strict = defaults.strict;
    if (!opts.lto && defaults.lto)
        opts.lto = defaults.lto;
    if (!opts.strip_debug && defaults.strip_debug)
        opts.strip_debug = defaults.strip_debug;
    if (opts.color == "auto" && defaults.color != "auto")
        opts.color = defaults.color;
    if (!opts.verbose && defaults.verbose)
        opts.verbose = defaults.verbose;
    if (opts.include_dirs.empty() && !defaults.include_dirs.empty())
        opts.include_dirs = defaults.include_dirs;
    if (opts.emit_target.empty() && !defaults.emit_target.empty())
        opts.emit_target = defaults.emit_target;

    opts.deriveTargetStage();
}

int main(int argc, char **argv) {
    auto opts = zith::cli::parseArgs(argc, argv);
    auto defaults = zith::cli::loadZithFlags();
    mergeFlags(opts, defaults);

    // TODO: try loading ZithProject.toml if no input file provided
    //       and merge with opts (project config overrides ZithFlags)

    zith::cli::CompilerDriver driver;
    return driver.run(opts);
}
