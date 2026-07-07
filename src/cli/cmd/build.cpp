#include "cli/commands.hpp"
#include "cli/terminal.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"

#include <cstdio>
#include <future>
#include <vector>

namespace zith::cli::commands {

namespace {

struct Printers {
    term::UsagePrinter out;
    term::UsagePrinter err;
};

Printers initPrinters(const Options &opts) {
    auto TERM = term::init(opts);
    return {{stdout, TERM.coutOn}, {stderr, TERM.cerrOn}};
}

void printCheckResults(const Printers &p, const std::vector<std::string> &files,
                       const std::vector<bool> &results, bool verbose) {
    size_t passed = countPassed(results);
    bool allPassed = (passed == files.size());

    if (verbose) {
        if (files.size() == 1) {
            results[0] ? p.out.green("[ok]") : p.out.red("[error]");
            std::printf(" %s\n", results[0] ? "check passed" : "check failed");
        } else {
            std::string list;
            for (size_t i = 0; i < files.size(); i++) {
                list += "\n\t";
                list += files[i];
                list += results[i] ? ": passed" : ": failed";
            }
            allPassed ? p.out.green("[ok]") : p.out.red("[error]");
            std::printf(" %zu/%zu passed%s\n", passed, files.size(), list.c_str());
        }
    } else {
        if (files.size() == 1) {
            results[0] ? p.out.green("[ok]") : p.out.red("[error]");
            std::printf(" %s\n", results[0] ? "check passed" : "check failed");
        } else {
            allPassed ? p.out.green("[ok]") : p.out.red("[error]");
            std::printf(" %zu/%zu passed\n", passed, files.size());
        }
    }
}

void printBuildResults(const Printers &p, const std::vector<std::string> &files,
                       const std::vector<bool> &results, bool verbose) {
    if (files.size() == 1) {
        if (verbose) {
            results[0] ? p.out.green("[ok]") : p.out.red("[error]");
            std::printf(" %s\n", files[0].c_str());
        }
    } else {
        for (size_t i = 0; i < files.size(); i++) {
            if (verbose) {
                results[i] ? p.out.green("[ok]") : p.out.red("[error]");
                std::printf(" %s\n", files[i].c_str());
            }
        }
    }
}

} // namespace

// ── Shared helpers (non-anonymous, declared in commands.hpp) ───────────────

static bool processFile(const Options &opts, const std::string &file, session::Stage stage) {
    session::CompilationSession session(opts, file);
    session.setBuffered(true);
    bool ok = (stage == session::Stage::Cached) ? session.run() : session.runTo(stage);
    session.emitDiagnostics();
    std::fputs(session.flushOutput().c_str(), stderr);
    return session.hasErrors() ? false : ok;
}

std::vector<std::string> collectFiles(const Options &opts) {
    std::vector<std::string> files;
    if (!opts.inputFiles.empty()) {
        files.reserve(opts.inputFiles.size());
        for (const auto &f : opts.inputFiles)
            files.push_back(f);
    }
    return files;
}

std::vector<bool> runOnFiles(const Options &opts, const std::vector<std::string> &files,
                             session::Stage stage) {
    std::vector<bool> results(files.size());

    if (files.size() == 1) {
        results[0] = processFile(opts, files[0], stage);
    } else {
        std::vector<std::future<bool>> futures;
        futures.reserve(files.size());
        for (const auto &file : files)
            futures.push_back(std::async(std::launch::async, [&opts, &file, stage]() {
                return processFile(opts, file, stage);
            }));
        for (size_t i = 0; i < futures.size(); i++)
            results[i] = futures[i].get();
    }

    return results;
}

size_t countPassed(const std::vector<bool> &results) {
    size_t c = 0;
    for (bool v : results)
        c += v;
    return c;
}

// ── check ──────────────────────────────────────────────────────────────────

int check(const Options &opts) {
    auto p = initPrinters(opts);
    auto files = collectFiles(opts);
    if (files.empty()) {
        p.err.red("[error]");
        std::fprintf(stderr, " no input files and no ZithProject.toml found\n");
        return 1;
    }
    auto results = runOnFiles(opts, files, session::Stage::TypeChecked);
    printCheckResults(p, files, results, opts.flags.verbose());
    return countPassed(results) == files.size() ? 0 : 1;
}

// ── build ──────────────────────────────────────────────────────────────────

int build(const Options &opts) {
    auto p = initPrinters(opts);
    auto files = collectFiles(opts);
    if (files.empty()) {
        p.err.red("[error]");
        std::fprintf(stderr, " no input files and no ZithProject.toml found\n");
        return 1;
    }
    auto results = runOnFiles(opts, files, session::Stage::Cached);
    printBuildResults(p, files, results, opts.flags.verbose());
    return countPassed(results) == files.size() ? 0 : 1;
}

} // namespace zith::cli::commands
