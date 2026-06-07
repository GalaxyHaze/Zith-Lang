#include "cli/commands.hpp"
#include "cli/compilation-session.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace zith::cli::commands {

namespace fs = std::filesystem;

int cmd_check(const Options &opts) {
    if (opts.input_files.empty()) {
        std::fprintf(stderr, "no input files\n");
        return 1;
    }

    std::vector<bool> results;
    results.reserve(opts.input_files.size());

    // TODO: parallelize with std::async / thread pool
    // Thread-safety: each CompilationSession owns independent state,
    // so they can run in parallel. SourceMap uses internal shared_mutex.
    for (const auto &file : opts.input_files) {
        CompilationSession session(opts, file);
        // check = lex + parse (syntax verification)
        bool ok = session.runTo(Stage::Parsed);
        if (session.hasErrors())
            ok = false;
        results.push_back(ok);
    }

    auto count = [](auto &&r) {
        size_t c = 0;
        for (bool v : r) c += v;
        return c;
    };

    if (opts.verbose) {
        if (opts.input_files.size() == 1) {
            std::printf("%s %s\n",
                        results[0] ? "[ok]" : "[error]",
                        results[0] ? "check passed" : "check failed");
        } else {
            std::string list;
            for (size_t i = 0; i < opts.input_files.size(); i++)
                list += "\n\t" + opts.input_files[i] + (results[i] ? ": passed" : ": failed");
            std::printf("%s %zu/%zu passed%s\n",
                        count(results) == opts.input_files.size() ? "[ok]" : "[error]",
                        count(results), opts.input_files.size(), list.c_str());
        }
    } else {
        if (opts.input_files.size() == 1) {
            std::printf("%s %s\n",
                        results[0] ? "[ok]" : "[error]",
                        results[0] ? "check passed" : "check failed");
        } else {
            std::printf("%s %zu/%zu passed\n",
                        count(results) == opts.input_files.size() ? "[ok]" : "[error]",
                        count(results), opts.input_files.size());
        }
    }

    return count(results) == opts.input_files.size() ? 0 : 1;
}

int cmd_compile(const Options &opts) {
    if (opts.input_files.empty()) {
        std::fprintf(stderr, "no input files\n");
        return 1;
    }

    int exit_code = 0;
    // TODO: parallelize with std::async / thread pool
    for (const auto &file : opts.input_files) {
        CompilationSession session(opts, file);
        // compile = up to MIR lowering (includes sema + HIR)
        bool ok = session.runTo(Stage::MirLowered);
        if (session.hasErrors()) {
            ok = false;
            exit_code = 1;
        }
        if (opts.verbose)
            std::printf("[%s] %s\n", ok ? "ok" : "error", file.c_str());
    }
    return exit_code;
}

int cmd_build(const Options &opts) {
    if (opts.input_files.empty()) {
        std::fprintf(stderr, "no input files\n");
        return 1;
    }

    int exit_code = 0;
    // TODO: parallelize with std::async / thread pool
    for (const auto &file : opts.input_files) {
        CompilationSession session(opts, file);
        // build = full pipeline through codegen/interpretation
        bool ok = session.run();
        if (session.hasErrors()) {
            ok = false;
            exit_code = 1;
        }
        if (opts.verbose)
            std::printf("[%s] %s\n", ok ? "ok" : "error", file.c_str());
    }
    return exit_code;
}

} // namespace zith::cli::commands
