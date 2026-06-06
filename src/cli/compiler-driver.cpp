#include "compiler-driver.hpp"
#include "cli/commands.hpp"

namespace zith::cli {

int CompilerDriver::run(const Options &opts) {
    using Command = Options::Command;

    switch (opts.command) {
    case Command::Help:
        return commands::cmd_help();
    case Command::Version:
        return commands::cmd_version();
    case Command::New:
        return commands::cmd_new(opts);
    case Command::Clean:
        return commands::cmd_clean(opts);
    case Command::Deps:
        return commands::cmd_deps(opts);
    case Command::Check:
        return commands::cmd_check(opts);
    case Command::Compile:
        return commands::cmd_compile(opts);
    case Command::Build:
        return commands::cmd_build(opts);
    case Command::Execute:
        return commands::cmd_execute(opts);
    case Command::Run:
        return commands::cmd_run(opts);
    case Command::Test:
        return commands::cmd_test(opts);
    case Command::Fmt:
        return commands::cmd_fmt(opts);
    case Command::Docs:
        return commands::cmd_docs(opts);
    case Command::Repl:
        return commands::cmd_repl(opts);
    case Command::None:
        // No subcommand — treat as help
        return commands::cmd_help();
    }
    return 1;
}

} // namespace zith::cli
