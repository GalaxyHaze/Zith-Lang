#include "cache/cache-paths.hpp"
#include "cache/cache.hpp"
#include "cli/commands.hpp"
#include "cli/docs/doc-errors.hpp"
#include "cli/docs/markdown-renderer.hpp"
#include "cli/docs/output-writer.hpp"
#include "cli/docs/project-graph.hpp"
#include "cli/terminal.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <toml++/toml.hpp>
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

// Determine project root from a path argument or current dir (walk up for ZithProject.toml)
static std::string findProjectRoot(const std::string &pathArg) {
    namespace fs = std::filesystem;
    if (pathArg.empty()) {
        auto cwd    = fs::current_path();
        auto search = cwd;
        while (true) {
            if (fs::exists(search / "ZithProject.toml"))
                return search.string();
            if (search == search.root_path())
                break;
            search = search.parent_path();
        }
        return cwd.string();
    }
    auto p = fs::weakly_canonical(fs::path(pathArg));
    if (fs::is_directory(p)) {
        auto search = p;
        while (true) {
            if (fs::exists(search / "ZithProject.toml"))
                return search.string();
            if (search == search.root_path())
                break;
            search = search.parent_path();
        }
        return p.string();
    }
    return p.parent_path().string();
}

// Read [dependencies] section from ZithProject.toml
static std::vector<std::pair<std::string, std::string>>
readDependencies(const std::string &tomlPath) {
    std::vector<std::pair<std::string, std::string>> deps;
    auto tbl = toml::parse_file(tomlPath);
    if (auto *depTable = tbl["dependencies"].as_table()) {
        for (auto &[key, val] : *depTable) {
            if (auto s = val.value<std::string>())
                deps.emplace_back(std::string(key.str()), *s);
        }
    }
    return deps;
}

int stubCommand(const Options &opts, const char *name) {
    auto TERM = term::init(opts);
    term::UsagePrinter err{stderr, TERM.cerrOn};
    err.yellow("[soon]");
    std::fprintf(stderr, " %s not implemented yet\n", name);
    return 1;
}

std::string findBuildDir(const Options &opts) {
    namespace fs = std::filesystem;
    std::string projectRoot;

    // If a file/dir was specified, use its parent
    if (!opts.subcommandStr.empty()) {
        auto path = fs::weakly_canonical(fs::path(opts.subcommandStr));
        if (fs::is_directory(path))
            projectRoot = path.string();
        else
            projectRoot = path.parent_path().string();
    } else {
        // Default to current directory
        projectRoot = fs::current_path().string();
    }

    // Check for ZithProject.toml to get binDir
    auto toml_path = fs::path(projectRoot) / "ZithProject.toml";
    if (fs::exists(toml_path)) {
        auto tbl = toml::parse_file(toml_path.string());
        if (auto *paths = tbl["paths"].as_table()) {
            if (auto v = paths->get("bin_dir"))
                if (auto s = v->value<std::string>())
                    return (fs::path(projectRoot) / *s).string();
        }
    }

    return (fs::path(projectRoot) / "build").string();
}

} // namespace

int test(const Options &opts) {
    auto p       = initPrinters(opts);
    namespace fs = std::filesystem;

    std::string pathArg;
    if (!opts.inputFiles.empty())
        pathArg = opts.inputFiles[0];
    auto projectRoot = findProjectRoot(pathArg);
    auto tomlPath    = fs::path(projectRoot) / "ZithProject.toml";

    // Determine test directory
    std::string testDir = (fs::path(projectRoot) / "tests").string();
    if (fs::exists(tomlPath)) {
        auto tbl = toml::parse_file(tomlPath.string());
        if (auto *paths = tbl["paths"].as_table()) {
            if (auto v = paths->get("test_dir"))
                if (auto s = v->value<std::string>())
                    testDir = (fs::path(projectRoot) / *s).string();
        }
    }

    if (!fs::exists(testDir) || !fs::is_directory(testDir)) {
        p.err.red("[error]");
        std::fprintf(stderr, " test directory not found: '%s'\n", testDir.c_str());
        return 1;
    }

    // Collect .zith test files
    std::vector<std::string> testFiles;
    for (const auto &entry : fs::recursive_directory_iterator(testDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".zith")
            testFiles.push_back(entry.path().string());
    }

    if (testFiles.empty()) {
        p.err.yellow("[info]");
        std::fprintf(stderr, " no .zith test files found in '%s'\n", testDir.c_str());
        return 1;
    }

    // Run each test through the check pipeline (TypeChecked = full sema)
    auto results   = runOnFiles(opts, testFiles, session::Stage::TypeChecked);
    size_t passed  = countPassed(results);
    bool allPassed = (passed == testFiles.size());

    allPassed ? p.out.green("[ok]") : p.out.red("[error]");
    std::printf(" %zu/%zu tests passed\n", passed, testFiles.size());

    if (opts.flags.verbose()) {
        for (size_t i = 0; i < testFiles.size(); i++) {
            if (!results[i]) {
                p.err.yellow("  FAIL");
                std::fprintf(stderr, " %s\n", testFiles[i].c_str());
            }
        }
    }

    return allPassed ? 0 : 1;
}

int docs(const Options &opts) {
    auto p = initPrinters(opts);

    if (opts.inputFiles.empty()) {
        p.err.red("[error]");
        std::fprintf(stderr, " no input files\n");
        return 1;
    }

    // Include semantic diagnostics before deciding whether partial output is allowed.
    session::CompilationSession session(opts, opts.inputFiles[0]);
    session.setBuffered(true);
    const bool pipelineOk = session.runTo(session::Stage::TypeChecked);
    session.emitDiagnostics();
    std::fputs(session.flushOutput().c_str(), stderr);

    const auto &snapshot = session.snapshot();
    if (!snapshot) {
        if (opts.docsIncludeErrors) {
            const std::vector<docs::DocError> errors = {
                {opts.inputFiles[0],
                 {},
                 "Compilation",
                 "failed to build compilation snapshot",
                 0,
                 0},
            };
            const auto errorMarkdown = docs::renderDocErrors(errors);
            if (opts.docsOut) {
                docs::DocModel emptyModel;
                emptyModel.entryModule = opts.inputFiles[0];
                const auto outputDirectory =
                    docs::resolveDocsOutputDirectory(opts.inputFiles[0], opts.docsOutPath);
                const auto result = docs::writeDocsOutput(
                    outputDirectory, emptyModel, opts.docsIndex, opts.docsForce, errorMarkdown);
                if (!result.ok || !result.message.empty()) {
                    p.err.red("[error]");
                    std::fprintf(stderr, " %s\n", result.message.c_str());
                }
            } else {
                std::fputs(errorMarkdown.c_str(), stdout);
            }
        }
        p.err.red("[error]");
        std::fprintf(stderr, " failed to build snapshot\n");
        return 1;
    }

    const auto graph = docs::buildProjectGraph(*snapshot);
    const auto mode =
        opts.docsMode == Options::DocsMode::Spec ? docs::DocsMode::Spec : docs::DocsMode::Interface;
    const auto model = docs::buildDocModel(*snapshot, graph, mode);
    auto errors      = docs::collectDocErrors(session, graph, model);
    const bool hasErrors =
        !pipelineOk || session.hasErrors() || !graph.ok() || !model.errors.empty();
    if (hasErrors && errors.empty()) {
        errors.push_back({opts.inputFiles[0], graph.entryModule, "Compilation",
                          "compilation or documentation generation failed", 0, 0});
    }
    if (hasErrors && !opts.docsIncludeErrors)
        return 1;

    const auto errorMarkdown =
        hasErrors && opts.docsIncludeErrors ? docs::renderDocErrors(errors) : std::string{};
    if (opts.docsOut) {
        const auto outputDirectory =
            docs::resolveDocsOutputDirectory(opts.inputFiles[0], opts.docsOutPath);
        const auto result = docs::writeDocsOutput(outputDirectory, model, opts.docsIndex,
                                                  opts.docsForce, errorMarkdown);
        if (!result.ok || !result.message.empty()) {
            p.err.red("[error]");
            std::fprintf(stderr, " %s\n", result.message.c_str());
            return 1;
        }
    } else {
        const auto markdown = docs::renderMarkdown(model);
        std::fputs(markdown.c_str(), stdout);
        std::fputs(errorMarkdown.c_str(), stdout);
    }

    if (hasErrors)
        return 1;
    return 0;
}

int repl(const Options &opts) {
    return stubCommand(opts, "repl");
}

int clean(const Options &opts) {
    auto TERM = term::init(opts);
    term::UsagePrinter out{stdout, TERM.coutOn};
    term::UsagePrinter err{stderr, TERM.cerrOn};
    namespace fs = std::filesystem;

    std::string projectRoot = findProjectRoot(opts.subcommandStr);

    // Compute build dir and cache dir
    std::string buildDir = findBuildDir(opts);
    auto cacheDir        = fs::path(projectRoot) / cache::kPersistentCacheDirName;

    bool cleaned = false;

    if (fs::exists(buildDir)) {
        fs::remove_all(buildDir);
        out.green("[ok]");
        std::printf(" removed '%s'\n", buildDir.c_str());
        cleaned = true;
    }

    if (fs::exists(cacheDir)) {
        fs::remove_all(cacheDir);
        out.green("[ok]");
        std::printf(" removed '%s'\n", cacheDir.string().c_str());
        cleaned = true;
    }

    if (!cleaned) {
        err.yellow("[info]");
        std::fprintf(stderr, " nothing to clean\n");
    }

    return cleaned ? 0 : 1;
}

int deps(const Options &opts) {
    auto p       = initPrinters(opts);
    namespace fs = std::filesystem;

    if (opts.subcommandStr.empty()) {
        p.err.red("[error]");
        std::fprintf(stderr, " no deps subcommand\n");
        std::fprintf(stderr,
                     "usage: zithc deps (list|add|remove|publish|unpublish|update) [args]\n");
        return 1;
    }

    auto sub = opts.subcommandStr;

    if (sub == "list") {
        auto projectRoot = findProjectRoot("");
        auto tomlPath    = fs::path(projectRoot) / "ZithProject.toml";

        if (!fs::exists(tomlPath)) {
            p.err.red("[error]");
            std::fprintf(stderr, " ZithProject.toml not found\n");
            return 1;
        }

        auto deps = readDependencies(tomlPath.string());
        if (deps.empty()) {
            p.out.green("[ok]");
            std::printf(" no dependencies\n");
            return 0;
        }

        p.out.green("[ok]");
        std::printf(" %zu dependencies:\n", deps.size());
        for (const auto &[name, version] : deps)
            std::printf("  %s = \"%s\"\n", name.c_str(), version.c_str());
        return 0;
    }

    if (sub == "add") {
        return stubCommand(opts, "deps add");
    }
    if (sub == "remove") {
        return stubCommand(opts, "deps remove");
    }
    if (sub == "publish" || sub == "unpublish" || sub == "update") {
        return stubCommand(opts, ("deps " + sub).c_str());
    }

    p.err.red("[error]");
    std::fprintf(stderr, " unknown deps subcommand '%s'\n", sub.c_str());
    std::fprintf(stderr, "usage: zithc deps (list|add|remove|publish|unpublish|update) [args]\n");
    return 1;
}

} // namespace zith::cli::commands
