#include "cli/commands.hpp"
#include "cli/pipeline-plan.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "lexer/lexer.hpp"

#include <cstdio>
#include <string>

namespace zith::cli::commands {

// TODO: implement native binary execution (fork+exec or embedded runner)
// TODO: implement interpreted bytecode execution via ZIR runtime
int cmd_execute(const Options &opts) {
    const std::string &target = opts.subcommand_arg.empty() && !opts.input_files.empty()
                                    ? opts.input_files[0]
                                    : opts.subcommand_arg;

    if (target.empty()) {
        std::fprintf(stderr, "error: no target specified and no ZithProject.toml found\n");
        return 1;
    }

    if (opts.verbose)
        std::printf("[*] Executing '%s'%s\n", target.c_str(),
                    opts.interpreted ? " (interpreted)" : "");

    std::fprintf(stderr, "[!] Execution not yet implemented\n");
    return 1;
}

int cmd_run(const Options &opts) {
    const std::string &src = opts.subcommand_arg.empty() && !opts.input_files.empty()
                                 ? opts.input_files[0]
                                 : opts.subcommand_arg;

    if (src.empty()) {
        std::fprintf(stderr, "error: no input file and no ZithProject.toml found\n");
        return 1;
    }

    if (opts.interpreted) {
        if (cmd_compile(opts) != 0)
            return 1;
        Options execOpts = opts;
        execOpts.subcommand_arg = opts.output_file.empty() ? "a.nbc" : opts.output_file;
        return cmd_execute(execOpts);
    }

    if (cmd_build(opts) != 0)
        return 1;

    Options execOpts = opts;
    execOpts.subcommand_arg = opts.output_file.empty() ? "a.out" : opts.output_file;
    return cmd_execute(execOpts);
}

} // namespace zith::cli::commands
