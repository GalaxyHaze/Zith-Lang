#include "cache/cache-paths.hpp"
#include "cache/cache.hpp"
#include "cli/commands.hpp"
#include "cli/docs/doc-errors.hpp"
#include "cli/docs/markdown-renderer.hpp"
#include "cli/docs/output-writer.hpp"
#include "cli/docs/project-graph.hpp"
#include "cli/terminal.hpp"
#include "common/test-log-protocol.hpp"
#include "interp/hir-interpreter.hpp"
#include "packages/lockfile.hpp"
#include "packages/resolver.hpp"
#include "session/compilation-session.hpp"
#include "session/pipeline-plan.hpp"
#ifdef ZITH_HAS_VM
#include "vm/hir-to-vm.hpp"
#include "vm/vm-v2.hpp"
#endif

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
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

struct TestResult {
    std::string name;
    uint32_t failedAssert = 0;
    std::vector<uint32_t> failedChecks;
};

constexpr std::string_view kTestResultsBegin = "zith-test-results-begin\n";
constexpr std::string_view kTestResultsEnd   = "zith-test-results-end\n";

std::string testModuleName(const std::filesystem::path &file,
                           const std::filesystem::path &test_dir) {
    auto relative = file.lexically_relative(test_dir);
    relative.replace_extension();
    auto result = relative.generic_string();
    for (size_t index = 0; index + 1U < result.size();) {
        if (result[index] == '/') {
            result.replace(index, 1U, "::");
            index += 2U;
        } else {
            ++index;
        }
    }
    return result;
}

bool parseTestProtocol(std::string_view output, const std::vector<std::string> &expected_names,
                       std::vector<TestResult> &tests, std::string &user_output) {
    if (!output.ends_with(kTestResultsEnd))
        return false;

    const size_t block_end = output.size() - kTestResultsEnd.size();
    tests.resize(expected_names.size());
    size_t cursor = block_end;
    for (size_t index = expected_names.size(); index > 0U; --index) {
        if (cursor == 0U || output[cursor - 1U] != '\n')
            return false;
        const size_t line_end = cursor - 1U;
        const size_t previous_newline =
            line_end == 0U ? std::string_view::npos : output.rfind('\n', line_end - 1U);
        const size_t line_start =
            previous_newline == std::string_view::npos ? 0U : previous_newline + 1U;
        const std::string_view line(output.data() + line_start, line_end - line_start);
        constexpr std::string_view result_prefix = "zith-test ";
        if (!line.starts_with(result_prefix))
            return false;
        const size_t value_separator = line.rfind(' ');
        if (value_separator == std::string_view::npos || value_separator <= result_prefix.size() ||
            line.substr(result_prefix.size(), value_separator - result_prefix.size()) !=
                expected_names[index - 1U])
            return false;
        uint32_t failed_assert = 0;
        const auto value_text  = line.substr(value_separator + 1U);
        const auto parsed = std::from_chars(value_text.data(),
                                            value_text.data() + value_text.size(), failed_assert);
        if (parsed.ec != std::errc{} || parsed.ptr != value_text.data() + value_text.size())
            return false;
        tests[index - 1U].name         = expected_names[index - 1U];
        tests[index - 1U].failedAssert = failed_assert;
        cursor                         = line_start;
    }

    if (cursor < kTestResultsBegin.size())
        return false;
    const size_t block_begin = cursor - kTestResultsBegin.size();
    if (output.substr(block_begin, kTestResultsBegin.size()) != kTestResultsBegin)
        return false;

    user_output.assign(output.substr(0, block_begin));
    user_output.append(output.substr(block_end + kTestResultsEnd.size()));
    return true;
}

void parseTestCheckFailures(std::string_view output, std::vector<TestResult> &tests,
                            std::string &user_output) {
    user_output.clear();
    user_output.reserve(output.size());
    size_t cursor = 0;
    while (cursor < output.size()) {
        const size_t begin     = output.find(common::kTestCheckFailurePrefix, cursor);
        const size_t log_begin = output.find(common::kTestLogBeginPrefix, cursor);
        if (log_begin != std::string_view::npos &&
            (begin == std::string_view::npos || log_begin < begin)) {
            const size_t header_end =
                output.find('\n', log_begin + common::kTestLogBeginPrefix.size());
            if (header_end == std::string_view::npos) {
                user_output.append(output.substr(cursor));
                return;
            }
            const size_t log_end = output.find(common::kTestLogEndMarker, header_end + 1U);
            if (log_end == std::string_view::npos) {
                user_output.append(output.substr(cursor));
                return;
            }
            const size_t block_end = log_end + common::kTestLogEndMarker.size();
            user_output.append(output.substr(cursor, block_end - cursor));
            cursor = block_end;
            continue;
        }
        if (begin == std::string_view::npos) {
            user_output.append(output.substr(cursor));
            return;
        }
        user_output.append(output.substr(cursor, begin - cursor));
        const size_t line_end = output.find('\n', begin);
        if (line_end == std::string_view::npos) {
            user_output.append(output.substr(begin));
            return;
        }

        const size_t record_start = begin + common::kTestCheckFailurePrefix.size();
        const std::string_view record(output.data() + record_start, line_end - record_start);
        const size_t separator = record.rfind(' ');
        uint32_t failure_id    = 0;
        bool consumed          = false;
        if (separator != std::string_view::npos && separator > 0U) {
            const auto id_text = record.substr(separator + 1U);
            const auto parsed =
                std::from_chars(id_text.data(), id_text.data() + id_text.size(), failure_id);
            if (parsed.ec == std::errc{} && parsed.ptr == id_text.data() + id_text.size() &&
                failure_id != 0U) {
                const std::string_view test_name = record.substr(0U, separator);
                for (auto &test : tests) {
                    if (test.name == test_name) {
                        test.failedChecks.push_back(failure_id);
                        consumed = true;
                        break;
                    }
                }
            }
        }
        if (!consumed)
            user_output.append(output.substr(begin, line_end + 1U - begin));
        cursor = line_end + 1U;
    }
}

std::string filterTestLogs(std::string_view output, const std::vector<TestResult> &tests,
                           bool verbose) {
    std::string filtered;
    filtered.reserve(output.size());
    std::vector<size_t> logged_bytes(tests.size(), 0U);
    size_t cursor = 0;

    while (cursor < output.size()) {
        const size_t begin = output.find(common::kTestLogBeginPrefix, cursor);
        if (begin == std::string_view::npos) {
            filtered.append(output.substr(cursor));
            break;
        }
        filtered.append(output.substr(cursor, begin - cursor));

        const size_t name_start = begin + common::kTestLogBeginPrefix.size();
        const size_t header_end = output.find('\n', name_start);
        if (header_end == std::string_view::npos) {
            filtered.append(output.substr(begin));
            break;
        }
        const size_t log_end = output.find(common::kTestLogEndMarker, header_end + 1U);
        if (log_end == std::string_view::npos) {
            filtered.append(output.substr(begin));
            break;
        }

        const std::string_view test_name(output.data() + name_start, header_end - name_start);
        size_t test_index = tests.size();
        for (size_t index = 0; index < tests.size(); ++index) {
            if (tests[index].name == test_name) {
                test_index = index;
                break;
            }
        }

        if (test_index == tests.size()) {
            filtered.append(
                output.substr(begin, log_end + common::kTestLogEndMarker.size() - begin));
        } else if (verbose || tests[test_index].failedAssert != 0U ||
                   !tests[test_index].failedChecks.empty()) {
            const size_t log_start   = header_end + 1U;
            const size_t log_size    = log_end - log_start;
            const size_t remaining   = common::kTestLogCapacity -
                                       std::min(logged_bytes[test_index], common::kTestLogCapacity);
            const size_t append_size = std::min(log_size, remaining);
            filtered.append(output.data() + log_start, append_size);
            logged_bytes[test_index] += append_size;
        }
        cursor = log_end + common::kTestLogEndMarker.size();
    }

    return filtered;
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

    // Collect .zith test files in deterministic order.
    std::vector<std::string> testFiles;
    for (const auto &entry : fs::recursive_directory_iterator(testDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".zith")
            testFiles.push_back(entry.path().string());
    }
    std::sort(testFiles.begin(), testFiles.end());

    if (testFiles.empty()) {
        p.err.yellow("[info]");
        std::fprintf(stderr, " no .zith test files found in '%s'\n", testDir.c_str());
        return 1;
    }

    size_t passed  = 0;
    size_t failed  = 0;
    size_t skipped = 0;
    for (const auto &file : testFiles) {
        if (opts.flags.verbose())
            std::fprintf(stderr, "[verbose] compiling test file '%s'\n", file.c_str());

        session::CompilationSession compilation(opts, file);
        compilation.setBuffered(true);
        compilation.setAlwaysEmitObject(true);
        compilation.setTestOutputCapture(true);
        compilation.setTestModuleName(testModuleName(file, testDir));

        const bool useInterpreter = opts.flags.interpreted();
        bool useVm                = opts.flags.virtualMachine() || useInterpreter;
#if !defined(ZITH_HAS_LLVM) || defined(ZITH_IS_WASM)
        useVm = true;
#endif
        const auto target   = useVm ? session::Stage::HirLowered : session::Stage::Cached;
        const bool compiled = compilation.runTo(target);
        compilation.emitDiagnostics();
        std::fputs(compilation.flushOutput().c_str(), stderr);
        if (!compiled) {
            p.err.red("[error]");
            std::fprintf(stderr, " failed to compile test file '%s'\n", file.c_str());
            ++failed;
            continue;
        }

        if (compilation.testNames().empty()) {
            if (opts.testFilter.empty()) {
                ++skipped;
                p.err.yellow("[skip]");
                std::fprintf(stderr, " no #[test] functions in '%s'\n", file.c_str());
            } else if (opts.flags.verbose()) {
                std::fprintf(stderr, "[verbose] no tests selected in '%s'\n", file.c_str());
            }
            continue;
        }

        if (opts.flags.verbose()) {
            std::fprintf(stderr, "[verbose] running %zu test(s) from '%s'\n",
                         compilation.testNames().size(), file.c_str());
        }

        std::string child_output;
        std::string execution_error;
        int child_exit = 0;
        bool executed  = true;
        if (useInterpreter) {
            interp::HirInterpreter interpreter(compilation.hirModule(), compilation.interner(),
                                               compilation.types());
            auto run        = interpreter.runMain();
            child_output    = std::move(run.output);
            child_exit      = static_cast<int>(run.exitCode);
            executed        = run.status == interp::HirInterpStatus::Ok;
            execution_error = std::move(run.message);
        } else if (useVm) {
#ifdef ZITH_HAS_VM
            memory::Arena vm_arena;
            vm::Module module(vm_arena);
            const auto lowered = vm::lowerModule(compilation.hirModule(), compilation.interner(),
                                                 compilation.types(), vm_arena, module);
            if (!lowered.ok) {
                p.err.red("[error]");
                std::fprintf(stderr, " could not lower '%s' to VM v2: %s\n", file.c_str(),
                             lowered.message.c_str());
                ++failed;
                continue;
            }
            vm::Vm machine;
            auto run        = machine.runMain(module);
            child_output    = std::move(run.output);
            child_exit      = static_cast<int>(run.exitCode);
            executed        = run.status == vm::RunStatus::Ok;
            execution_error = std::move(run.message);
#else
            p.err.red("[error]");
            std::fprintf(stderr, " this build excludes the VM v2 backend\n");
            ++failed;
            continue;
#endif
        } else {
            executed     = compilation.linkAndExec();
            child_output = compilation.takeChildOutput();
            child_exit   = compilation.childExitCode();
            if (!executed) {
                const auto execution_output = compilation.flushOutput();
                if (!execution_output.empty())
                    std::fwrite(execution_output.data(), 1U, execution_output.size(), stderr);
            }
        }

        std::vector<TestResult> results;
        std::string user_output;
        const auto &expected_names = compilation.testNames();
        const bool has_protocol =
            parseTestProtocol(child_output, expected_names, results, user_output);
        if (!has_protocol) {
            user_output = child_output;
        } else {
            std::string without_check_events;
            parseTestCheckFailures(user_output, results, without_check_events);
            user_output = std::move(without_check_events);
            user_output = filterTestLogs(user_output, results, opts.flags.verbose());
        }
        if (!user_output.empty())
            std::fwrite(user_output.data(), 1U, user_output.size(), stdout);
        if (!has_protocol || !executed || child_exit != 0) {
            p.err.red("[error]");
            std::fprintf(stderr, " test process failed for '%s' (exit %d)\n", file.c_str(),
                         child_exit);
            if (!execution_error.empty())
                std::fprintf(stderr, "    %s\n", execution_error.c_str());
            ++failed;
            continue;
        }

        for (const auto &result : results) {
            const bool passed_test = result.failedAssert == 0U && result.failedChecks.empty();
            if (passed_test) {
                ++passed;
                p.out.green("  PASS");
            } else {
                ++failed;
                p.out.red("  FAIL");
            }
            std::printf(" %s\n", result.name.c_str());
            const auto separator = result.name.rfind("::");
            const std::string_view bare_name =
                separator == std::string::npos
                    ? std::string_view(result.name)
                    : std::string_view(result.name).substr(separator + 2U);
            for (const auto failed_check : result.failedChecks) {
                for (const auto &site : compilation.testFailureSites()) {
                    if (site.test_name != bare_name || site.failure_id != failed_check)
                        continue;
                    const auto location = compilation.sourceMap().loc(site.span);
                    if (location.line != 0)
                        std::fprintf(stderr, "    %s:%u:%u: check failed\n", file.c_str(),
                                     location.line, location.col);
                    break;
                }
            }
            if (result.failedAssert != 0U) {
                for (const auto &site : compilation.testFailureSites()) {
                    if (site.test_name != bare_name || site.failure_id != result.failedAssert)
                        continue;
                    const auto location = compilation.sourceMap().loc(site.span);
                    if (location.line != 0)
                        std::fprintf(stderr, "    %s:%u:%u: assertion failed\n", file.c_str(),
                                     location.line, location.col);
                    break;
                }
            }
        }
    }

    const size_t total = passed + failed;
    if (total == 0U) {
        p.err.yellow("[info]");
        if (!opts.testFilter.empty()) {
            std::fprintf(stderr, " no tests matched filter '%s'\n", opts.testFilter.c_str());
            return 1;
        }
        if (skipped > 0U) {
            std::fprintf(stderr, " no tests found under '%s'\n", testDir.c_str());
            return 0;
        }
        std::fprintf(stderr, " no tests found under '%s'\n", testDir.c_str());
        return 1;
    }
    (failed == 0U) ? p.out.green("[ok]") : p.out.red("[error]");
    std::printf(" %zu passed; %zu failed; %zu total\n", passed, failed, total);
    return failed == 0U ? 0 : 1;
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
        std::fprintf(
            stderr,
            "usage: zithc deps (list|add|remove|resolve|publish|unpublish|update) [args]\n");
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

    if (sub == "resolve") {
        const auto projectRoot  = fs::path(findProjectRoot(""));
        const auto manifestPath = projectRoot / "ZithProject.toml";
        auto lockfile           = packages::resolveLocalPackages(manifestPath);
        if (!lockfile) {
            p.err.red("[error]");
            std::fprintf(stderr, " %s\n", lockfile.error().msg.c_str());
            return 1;
        }

        auto serialized = packages::serializeLockfile(lockfile.value());
        if (!serialized) {
            p.err.red("[error]");
            std::fprintf(stderr, " %s\n", serialized.error().msg.c_str());
            return 1;
        }

        const auto lockfilePath = projectRoot / "Zith.lock";
        std::ofstream output(lockfilePath, std::ios::binary | std::ios::trunc);
        if (!output) {
            p.err.red("[error]");
            std::fprintf(stderr, " cannot write lockfile '%s'\n", lockfilePath.string().c_str());
            return 1;
        }

        output << serialized.value();
        output.close();
        if (!output) {
            p.err.red("[error]");
            std::fprintf(stderr, " failed while writing lockfile '%s'\n",
                         lockfilePath.string().c_str());
            return 1;
        }

        p.out.green("[ok]");
        std::printf(" resolved %zu packages into '%s'\n", lockfile.value().packages.size(),
                    lockfilePath.string().c_str());
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
    std::fprintf(stderr,
                 "usage: zithc deps (list|add|remove|resolve|publish|unpublish|update) [args]\n");
    return 1;
}

} // namespace zith::cli::commands
