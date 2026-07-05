#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"

#include <cstdio>
#include <future>
#include <memory>
#include <vector>

namespace zith::cli::commands {

namespace {

struct SessionResult {
    std::unique_ptr<session::CompilationSession> session;
    bool ok;
};

} // namespace

// ── check ──────────────────────────────────────────────────────────────────

int check(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter out{stdout, TERM.coutOn};
    term::UsagePrinter err{stderr, TERM.cerrOn};

    std::vector<std::string> files;
    if (opts.inputFiles.empty()) {
        files.push_back(".");
    } else {
        files.reserve(opts.inputFiles.size());
        for (const auto &f : opts.inputFiles)
            files.push_back(f);
    }

    std::vector<bool> results;
    results.reserve(files.size());

    if (files.size() == 1) {
        session::CompilationSession session(opts, files[0]);
        session.setBuffered(true);
        bool ok = session.runTo(session::Stage::TypeChecked);
        session.emitDiagnostics();
        std::fputs(session.flushOutput().c_str(), stderr);
        if (session.hasErrors())
            ok = false;
        results.push_back(ok);
    } else {
        std::vector<std::future<SessionResult>> futures;
        futures.reserve(files.size());
        for (const auto &file : files) {
            futures.push_back(std::async(std::launch::async, [&opts, file]() -> SessionResult {
                auto session = std::make_unique<session::CompilationSession>(opts, file);
                session->setBuffered(true);
                bool ok = session->runTo(session::Stage::TypeChecked);
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

    auto okCount = [&results]() -> size_t {
        size_t c = 0;
        for (bool v : results)
            c += v;
        return c;
    };

    if (opts.flags.verbose()) {
        if (files.size() == 1) {
            results[0] ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", results[0] ? "check passed" : "check failed");
        } else {
            std::string list;
            for (size_t i = 0; i < files.size(); i++) {
                list += "\n\t";
                list += files[i];
                list += results[i] ? ": passed" : ": failed";
            }
            (okCount() == files.size() ? out.green("[ok]") : out.red("[error]"));
            std::printf(" %zu/%zu passed%s\n", okCount(), files.size(), list.c_str());
        }
    } else {
        if (files.size() == 1) {
            results[0] ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", results[0] ? "check passed" : "check failed");
        } else {
            (okCount() == files.size() ? out.green("[ok]") : out.red("[error]"));
            std::printf(" %zu/%zu passed\n", okCount(), files.size());
        }
    }

    return okCount() == files.size() ? 0 : 1;
}

// ── compile ────────────────────────────────────────────────────────────────

int compile(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter out{stdout, TERM.coutOn};
    term::UsagePrinter err{stderr, TERM.cerrOn};

    if (opts.inputFiles.empty()) {
        err.red("[error]");
        std::fprintf(stderr, " no input files\n");
        return 1;
    }

    if (opts.inputFiles.size() == 1) {
        const auto &file = opts.inputFiles[0];
        session::CompilationSession session(opts, file);
        session.setBuffered(true);
        bool ok = session.runTo(session::Stage::CodegenReady);
        session.emitDiagnostics();
        std::fputs(session.flushOutput().c_str(), stderr);
        if (opts.flags.verbose()) {
            ok ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", file.c_str());
        }
        return ok ? 0 : 1;
    }

    int exit_code = 0;
    std::vector<std::future<SessionResult>> futures;
    futures.reserve(opts.inputFiles.size());
    for (const auto &file : opts.inputFiles) {
        futures.push_back(std::async(std::launch::async, [&opts, file]() -> SessionResult {
            auto session = std::make_unique<session::CompilationSession>(opts, file);
            session->setBuffered(true);
            bool ok = session->runTo(session::Stage::CodegenReady);
            return {std::move(session), ok};
        }));
    }
    for (auto &f : futures) {
        auto sr = f.get();
        sr.session->emitDiagnostics();
        std::fputs(sr.session->flushOutput().c_str(), stderr);
        if (opts.flags.verbose()) {
            sr.ok ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", sr.session->filePath().c_str());
        }
        if (!sr.ok)
            exit_code = 1;
    }
    return exit_code;
}

// ── build ──────────────────────────────────────────────────────────────────

int build(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter out{stdout, TERM.coutOn};
    term::UsagePrinter err{stderr, TERM.cerrOn};

    if (opts.inputFiles.empty()) {
        err.red("[error]");
        std::fprintf(stderr, " no input files\n");
        return 1;
    }

    if (opts.inputFiles.size() == 1) {
        const auto &file = opts.inputFiles[0];
        session::CompilationSession session(opts, file);
        session.setBuffered(true);
        bool ok = session.run();
        session.emitDiagnostics();
        std::fputs(session.flushOutput().c_str(), stderr);
        if (session.hasErrors())
            ok = false;
        if (opts.flags.verbose()) {
            ok ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", file.c_str());
        }
        return ok ? 0 : 1;
    }

    int exit_code = 0;
    std::vector<std::future<SessionResult>> futures;
    futures.reserve(opts.inputFiles.size());
    for (const auto &file : opts.inputFiles) {
        futures.push_back(std::async(std::launch::async, [&opts, file]() -> SessionResult {
            auto session = std::make_unique<session::CompilationSession>(opts, file);
            session->setBuffered(true);
            bool ok = session->run();
            return {std::move(session), ok};
        }));
    }
    for (auto &f : futures) {
        auto sr = f.get();
        sr.session->emitDiagnostics();
        std::fputs(sr.session->flushOutput().c_str(), stderr);
        if (opts.flags.verbose()) {
            sr.ok ? out.green("[ok]") : out.red("[error]");
            std::printf(" %s\n", sr.session->filePath().c_str());
        }
        if (!sr.ok)
            exit_code = 1;
    }
    return exit_code;
}

} // namespace zith::cli::commands
