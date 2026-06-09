#include "cli/commands.hpp"
#include "cli/compilation-session.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace zith::cli::commands {

    static bool useColor(const Options &opts) {
        if (opts.color == "on") return true;
        if (opts.color == "off") return false;
        return isatty(fileno(stdout)) != 0;
    }

    static const char *green(const Options &opts) {
        return useColor(opts) ? "\033[32m" : "";
    }
    static const char *red(const Options &opts) {
        return useColor(opts) ? "\033[31m" : "";
    }
    static const char *reset(const Options &opts) {
        return useColor(opts) ? "\033[0m" : "";
    }


int cmd_check(const Options &opts) {
    memory::DynArray<std::string> files{memory::SessionArena};

    if (opts.input_files.empty()) {
        files.push(".");
    } else {
        for (const auto &f : opts.input_files)
            files.push(f);
    }

    memory::DynArray<bool> results{memory::SessionArena};
    results.reserve(files.size());

    // TODO: parallelize with std::async / thread pool
    // Thread-safety: each CompilationSession owns independent state,
    // so they can run in parallel. SourceMap uses internal shared_mutex.
    for (const auto &file : files) {
        CompilationSession session(opts, file);
        // check = lex + parse (syntax verification)
        bool ok = session.runTo(Stage::Parsed);
        if (session.hasErrors())
            ok = false;
        results.push(ok);
    }

    auto count = [](auto &&r) {
        size_t c = 0;
        for (bool v : r) c += v;
        return c;
    };

    auto ok_tag = [&](bool ok) {
        std::printf("%s[%s]%s", ok ? green(opts) : red(opts),
                    ok ? "ok" : "error", reset(opts));
    };

    if (opts.verbose) {
        if (files.size() == 1) {
            ok_tag(results[0]);
            std::printf(" %s\n", results[0] ? "check passed" : "check failed");
        } else {
            std::string list;
            for (size_t i = 0; i < files.size(); i++) {
                list += "\n\t";
                list += files[i];
                list += results[i] ? ": passed" : ": failed";
            }
            ok_tag(count(results) == files.size());
            std::printf(" %zu/%zu passed%s\n", count(results), files.size(), list.c_str());
        }
    } else {
        if (files.size() == 1) {
            ok_tag(results[0]);
            std::printf(" %s\n", results[0] ? "check passed" : "check failed");
        } else {
            ok_tag(count(results) == files.size());
            std::printf(" %zu/%zu passed\n", count(results), files.size());
        }
    }

    return count(results) == files.size() ? 0 : 1;
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
        if (opts.verbose) {
            std::printf("%s[%s]%s %s\n",
                        ok ? green(opts) : red(opts),
                        ok ? "ok" : "error", reset(opts), file.c_str());
        }
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
        if (opts.verbose) {
            std::printf("%s[%s]%s %s\n",
                        ok ? green(opts) : red(opts),
                        ok ? "ok" : "error", reset(opts), file.c_str());
        }
    }
    return exit_code;
}

} // namespace zith::cli::commands
