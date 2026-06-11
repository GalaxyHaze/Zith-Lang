#pragma once

#include "cli/pipeline-plan.hpp"
#include "cli/project-config.hpp"

#include <string>
#include <vector>

namespace zith::cli {

struct Options {
    // Input / output
    std::vector<std::string> input_files;
    std::string output_file = "a.out";
    std::vector<std::string> include_dirs;

    // Build configuration
    std::string mode = "debug";
    std::string target_triple;
    int opt_level     = 0;
    int debug_level   = 2;
    bool strict       = false;
    bool lto          = false;
    bool strip_debug  = false;
    std::string color = "auto";

    // Emit / dump flags
    bool emit_tokens  = false;
    bool emit_ast     = false;
    bool emit_hir     = false;
    bool emit_mir     = false;
    bool emit_ir      = false;
    bool emit_asm     = false;
    bool print_tokens = false;

    // Emit target string (from --emit flag)
    std::string emit_target;

    // Derived: pipeline target stage
    Stage target_stage = Stage::ZirInterpreted;
    bool interpreted   = false;

    // Behaviour flags
    bool verbose = false;

    // Subcommand
    enum class Command {
        None,
        Build,
        Run,
        Check,
        Compile,
        Execute,
        Test,
        Fmt,
        Docs,
        Repl,
        New,
        Clean,
        Deps,
        Version,
        Help
    };
    Command command = Command::None;
    std::string subcommand_arg;

    void deriveTargetStage();
};

Options parseArgs(int argc, char **argv);

// Merge ZithProject.toml build settings into Options.
// CLI flags take priority — project config only fills in unset values.
void mergeProjectConfig(Options &opts, const ProjectConfig &cfg);

} // namespace zith::cli
