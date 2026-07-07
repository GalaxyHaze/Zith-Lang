#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"

#include <cstdio>
#include <future>

namespace zith::cli::commands {

int execute(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter out{stdout, TERM.coutOn};
    term::UsagePrinter err{stderr, TERM.cerrOn};

    auto files = collectFiles(opts);
    if (files.empty()) {
        err.red("[error]");
        std::fprintf(stderr, " no input files and no ZithProject.toml found\n");
        return 1;
    }

    auto results = runOnFiles(opts, files, session::Stage::Cached);
    bool allPassed = (countPassed(results) == files.size());

    if (opts.flags.verbose()) {
        for (size_t i = 0; i < files.size(); i++) {
            results[i] ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", files[i].c_str());
        }
    }

    if (!allPassed)
        return 1;

    err.yellow("[soon]");
    std::fprintf(stderr, " execution not implemented yet\n");
    return 1;
}

int run(const Options &opts) {
    return execute(opts);
}

} // namespace zith::cli::commands
