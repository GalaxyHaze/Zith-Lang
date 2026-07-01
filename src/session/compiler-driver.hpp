#pragma once

#include "cli/commands.hpp"
#include "cli/options.hpp"

namespace zith::cli {

class CompilerDriver {
public:
    static int run(const Options &opts) {
        using Command = Options::Command;

        switch (opts.command) {
        case Command::Help:
            return commands::help();
        case Command::Version:
            return commands::version();
        case Command::Create:
            return commands::create(opts);
        case Command::Clean:
            return commands::clean(opts);
        case Command::Deps:
            return commands::deps(opts);
        case Command::Check:
            return commands::check(opts);
        case Command::Compile:
            return commands::compile(opts);
        case Command::Build:
            return commands::build(opts);
        case Command::Execute:
            return commands::execute(opts);
        case Command::Run:
            return commands::run(opts);
        case Command::Test:
            return commands::test(opts);
        case Command::Fmt:
            return commands::fmt(opts);
        case Command::Docs:
            return commands::docs(opts);
        case Command::Repl:
            return commands::repl(opts);
        case Command::None:
            // No subcommand — treat as help
            return commands::help();
        }
        return 1;
    }
};

} // namespace zith::cli
