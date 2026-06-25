#include "cli/commands.hpp"
#include "cli/compilation-session.hpp"
#include "cli/files.hpp"
#include "cli/terminal.hpp"
#include "diagnostics/color.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#elif !defined(ZITH_IS_WASM)
#include <unistd.h>
#endif

namespace zith::cli::commands {

#define CERR(c) term::err(TERM, diagnostics::ansi::c.data())
#define RERR   term::err_rst(TERM)
#define COUT(c) term::out(TERM, diagnostics::ansi::c.data())
#define ROUT   term::out_rst(TERM)

static bool isStdinTerminal() {
#ifdef ZITH_IS_WASM
    return false;
#elif _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(fileno(stdin)) != 0;
#endif
}

static std::string readStdin() {
    std::string content;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0)
        content.append(buf, n);
    return content;
}

static std::string readFile(const std::string &path) {
    std::ifstream file(path);
    if (!file)
        return {};
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

static bool writeFile(const std::string &path, const std::string &content) {
    std::ofstream file(path);
    if (!file)
        return false;
    file << content;
    return file.good();
}

struct FmtFileResult {
    std::unique_ptr<CompilationSession> session;
    std::string formatted;
    bool ok;
};

int cmd_fmt(const Options &opts) {
    auto TERM = term::init(opts);
    std::vector<std::string> files;
    bool has_stdin = false;

    if (opts.input_files.empty()) {
        if (!isStdinTerminal()) {
            has_stdin = true;
        } else {
            files = cli::findZithFiles(".");
            if (files.empty()) {
                std::fprintf(stderr, "%s[error]%s no .zith files found\n", CERR(red), RERR);
                return 1;
            }
        }
    } else {
        for (const auto &f : opts.input_files) {
            if (f == "-")
                has_stdin = true;
            else
                files.push_back(f);
        }
    }

    // Stdin mode
    if (has_stdin) {
        if (opts.fmt_check) {
            std::fprintf(stderr, "%s[error]%s --check is not supported with stdin\n",
                         CERR(red), RERR);
            return 1;
        }
        if (opts.fmt_in_place) {
            std::fprintf(stderr, "%s[error]%s -i/--in-place is not supported with stdin\n",
                         CERR(red), RERR);
            return 1;
        }

        Options file_opts = opts;
        file_opts.interpreted = false;
        CompilationSession session(file_opts, "<stdin>");
        session.setContent(readStdin());
        session.setBuffered(true);

        std::string formatted = session.fmtStage();
        if (session.hasErrors()) {
            session.emitDiagnostics();
            return 1;
        }
        std::printf("%s", formatted.c_str());
        return 0;
    }

    // File mode
    int exit_code = 0;
    bool any_diff = false;

    // Parallel formatting
    std::vector<std::future<FmtFileResult>> futures;
    futures.reserve(files.size());
    for (const auto &file : files) {
        futures.push_back(std::async(std::launch::async, [&opts, file]() -> FmtFileResult {
            Options file_opts = opts;
            file_opts.interpreted = false;
            auto session = std::make_unique<CompilationSession>(file_opts, file);
            session->setBuffered(true);
            std::string formatted = session->fmtStage();
            bool ok = !session->hasErrors();
            return {std::move(session), std::move(formatted), ok};
        }));
    }

    for (size_t i = 0; i < files.size(); i++) {
        auto result = futures[i].get();

        if (!result.ok) {
            std::fprintf(stderr, "%s[error]%s failed to format %s\n", CERR(red), RERR,
                         files[i].c_str());
            result.session->emitDiagnostics();
            exit_code = 1;
            continue;
        }

        if (opts.fmt_check) {
            std::string original = readFile(files[i]);
            if (result.formatted != original) {
                std::fprintf(stderr, "%s[check]%s %s would be reformatted\n", CERR(yellow),
                             RERR, files[i].c_str());
                any_diff = true;
                exit_code = 1;
            } else {
                std::fprintf(stdout, "%s[ok]%s %s\n", COUT(green), ROUT, files[i].c_str());
            }
        } else if (opts.fmt_in_place) {
            if (!writeFile(files[i], result.formatted)) {
                std::fprintf(stderr, "%s[error]%s failed to write %s\n", CERR(red), RERR,
                             files[i].c_str());
                exit_code = 1;
            } else if (opts.verbose) {
                std::fprintf(stdout, "%s[fmt]%s %s\n", COUT(green), ROUT, files[i].c_str());
            }
        } else {
            if (i > 0)
                std::printf("\n");
            std::printf("%s", result.formatted.c_str());
        }
    }

    if (any_diff && opts.fmt_check) {
        std::fprintf(stderr, "%s[check]%s some files need reformatting\n", CERR(yellow), RERR);
        std::fprintf(stderr, "%s[check]%s run `zithc fmt -i` to reformat\n", CERR(yellow), RERR);
    }

    return exit_code;
}

} // namespace zith::cli::commands
