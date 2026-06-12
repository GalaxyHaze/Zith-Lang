#include "cli/commands.hpp"
#include "cli/compilation-session.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace zith::cli::commands {

struct SessionResult {
    std::unique_ptr<CompilationSession> session;
    bool ok;
};

static bool useColor(const Options &opts) {
    if (opts.color == "on")
        return true;
    if (opts.color == "off")
        return false;
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
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
    std::vector<std::string> files;

    if (opts.input_files.empty()) {
        files.push_back(".");
    } else {
        for (const auto &f : opts.input_files)
            files.push_back(f);
    }

    std::vector<bool> results;
    results.reserve(files.size());

    if (files.size() == 1) {
        // Single file: sequential, immediate output
        CompilationSession session(opts, files[0]);
        bool ok = session.runTo(Stage::Parsed);
        if (session.hasErrors())
            ok = false;
        results.push_back(ok);
    } else {
        // Multi-file: parallel with buffered output
        std::vector<std::future<SessionResult>> futures;
        futures.reserve(files.size());
        for (const auto &file : files) {
            futures.push_back(std::async(std::launch::async, [&opts, file]() -> SessionResult {
                auto session = std::make_unique<CompilationSession>(opts, file);
                session->setBuffered(true);
                bool ok = session->runTo(Stage::Parsed);
                return {std::move(session), ok};
            }));
        }
        for (auto &f : futures) {
            auto sr = f.get();
            sr.session->emitDiagnostics();
            std::fputs(sr.session->flushOutput().c_str(), stderr);
            results.push_back(sr.ok);
        }
    }

    auto count = [](auto &&r) {
        size_t c = 0;
        for (bool v : r)
            c += v;
        return c;
    };

    auto ok_tag = [&](bool ok) {
        std::printf("%s[%s]%s", ok ? green(opts) : red(opts), ok ? "ok" : "error", reset(opts));
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
        std::fprintf(stderr, "%sno input files%s\n", red(opts), reset(opts));
        return 1;
    }

    if (opts.input_files.size() == 1) {
        const auto &file = opts.input_files[0];
        CompilationSession session(opts, file);
        session.setBuffered(true);
        bool ok = session.runTo(Stage::MirLowered);
        session.emitDiagnostics();
        std::fputs(session.flushOutput().c_str(), stderr);
        if (opts.verbose) {
            std::printf("%s[%s]%s %s\n", ok ? green(opts) : red(opts), ok ? "ok" : "error",
                        reset(opts), file.c_str());
        }
        return ok ? 0 : 1;
    }

    int exit_code = 0;
    std::vector<std::future<SessionResult>> futures;
    futures.reserve(opts.input_files.size());
    for (const auto &file : opts.input_files) {
        futures.push_back(std::async(std::launch::async, [&opts, file]() -> SessionResult {
            auto session = std::make_unique<CompilationSession>(opts, file);
            session->setBuffered(true);
            bool ok = session->runTo(Stage::MirLowered);
            return {std::move(session), ok};
        }));
    }
    for (auto &f : futures) {
        auto sr = f.get();
        sr.session->emitDiagnostics();
        std::fputs(sr.session->flushOutput().c_str(), stderr);
        if (opts.verbose) {
            std::printf("%s[%s]%s %s\n", sr.ok ? green(opts) : red(opts),
                        sr.ok ? "ok" : "error", reset(opts),
                        sr.session->filePath().c_str());
        }
        if (!sr.ok)
            exit_code = 1;
    }
    return exit_code;
}

int cmd_build(const Options &opts) {
    if (opts.input_files.empty()) {
        std::fprintf(stderr, "%sno input files%s\n", red(opts), reset(opts));
        return 1;
    }

    if (opts.input_files.size() == 1) {
        // Single file: sequential, immediate output
        const auto &file = opts.input_files[0];
        CompilationSession session(opts, file);
        bool ok = session.run();
        if (session.hasErrors()) {
            ok = false;
        }
        if (opts.verbose) {
            std::printf("%s[%s]%s %s\n", ok ? green(opts) : red(opts), ok ? "ok" : "error",
                        reset(opts), file.c_str());
        }
        return ok ? 0 : 1;
    }

    int exit_code = 0;
    std::vector<std::future<SessionResult>> futures;
    futures.reserve(opts.input_files.size());
    for (const auto &file : opts.input_files) {
        futures.push_back(std::async(std::launch::async, [&opts, file]() -> SessionResult {
            auto session = std::make_unique<CompilationSession>(opts, file);
            session->setBuffered(true);
            bool ok = session->run();
            return {std::move(session), ok};
        }));
    }
    for (auto &f : futures) {
        auto sr = f.get();
        sr.session->emitDiagnostics();
        std::fputs(sr.session->flushOutput().c_str(), stderr);
        if (opts.verbose) {
            std::printf("%s[%s]%s %s\n", sr.ok ? green(opts) : red(opts),
                        sr.ok ? "ok" : "error", reset(opts),
                        sr.session->filePath().c_str());
        }
        if (!sr.ok)
            exit_code = 1;
    }
    return exit_code;
}

} // namespace zith::cli::commands
