#include "cli/commands.hpp"
#include "cli/compilation-session.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace zith::cli::commands {

static bool useTermColor(const Options &opts, FILE *out) {
    if (opts.color == "on")
        return true;
    if (opts.color == "off")
        return false;
#ifdef _WIN32
    return _isatty(_fileno(out)) != 0;
#else
    return isatty(fileno(out)) != 0;
#endif
}
#define CERR(c) (useTermColor(opts, stderr) ? diagnostics::ansi::c.data() : "")
#define RERR (useTermColor(opts, stderr) ? diagnostics::ansi::reset.data() : "")
#define COUT(c) (useTermColor(opts, stdout) ? diagnostics::ansi::c.data() : "")
#define ROUT (useTermColor(opts, stdout) ? diagnostics::ansi::reset.data() : "")

int cmd_execute(const Options &opts) {
    if (opts.input_files.empty()) {
        std::fprintf(stderr, "%sno input files%s\n", CERR(red), RERR);
        return 1;
    }

    // TODO: execute = full pipeline + run interpreted binary
    // For now, run the pipeline to completion (ZirInterpreted)
    for (const auto &file : opts.input_files) {
        CompilationSession session(opts, file);
        bool ok = session.run();
        if (session.hasErrors())
            ok = false;
        if (opts.verbose)
            std::printf("%s[%s]%s %s\n", ok ? COUT(green) : COUT(red), ok ? "ok" : "error", ROUT,
                        file.c_str());
        if (!ok)
            return 1;
    }

    // TODO: execute the compiled output / interpreted module
    std::fprintf(stderr, "%s[soon]%s execution not implemented yet\n", CERR(yellow), RERR);
    return 1;
}

int cmd_run(const Options &opts) {
    // run = compile + execute in one step
    return cmd_execute(opts);
}

} // namespace zith::cli::commands
