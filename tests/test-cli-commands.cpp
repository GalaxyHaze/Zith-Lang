#include "cli/commands.hpp"
#include "cli/options.hpp"
#include "session/project-options-merge.hpp"
#include "test-common.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

using namespace zith;
using namespace zith::cli::commands;

namespace {

struct CliCapture {
    struct Result {
        int exitCode = -1;
        std::string stdoutText;
        std::string stderrText;
    };

    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("zith-docs-cli-" +
         std::to_string(std::filesystem::file_time_type::clock::now().time_since_epoch().count()));

    ~CliCapture() {
        std::filesystem::remove_all(root);
    }

    Result run(const std::string &args) const {
        std::filesystem::create_directories(root);
        const auto stdoutPath     = root / "stdout.txt";
        const auto stderrPath     = root / "stderr.txt";
        const std::string command = std::string("\"") + ZITHC_BINARY + "\" " + args + " > \"" +
                                    stdoutPath.string() + "\" 2> \"" + stderrPath.string() + "\"";
        const int status          = std::system(command.c_str());
        int exitCode              = status;
#ifndef _WIN32
        exitCode = (status != -1 && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
#endif

        const auto read = [](const std::filesystem::path &path) {
            std::ifstream input(path, std::ios::binary);
            return std::string((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
        };
        return {exitCode, read(stdoutPath), read(stderrPath)};
    }

    std::string run(const std::string &args, const std::string &stream) const {
        const auto result = run(args);
        return std::to_string(result.exitCode) + "\n" +
               (stream == "stdout" ? result.stdoutText : result.stderrText);
    }
};

// ── Options parsing validation ────────────────────────────────────

static void test_options_defaults() {
    memory::Arena arena;
    Options opts(arena);

    // command defaults to None
    CHECK(opts.command == Options::Command::None, "default command is None");

    // input files start empty
    CHECK(opts.inputFiles.empty(), "default inputFiles is empty");

    // include dirs start empty
    CHECK(opts.includeDirs.empty(), "default includeDirs is empty");

    CHECK(!opts.flags.debugSema(), "debug-sema defaults to disabled");
}

static void test_options_command_enum() {
    memory::Arena arena;
    Options opts(arena);

    opts.command = Options::Command::Build;
    CHECK(opts.command == Options::Command::Build, "Command::Build is set");

    opts.command = Options::Command::Test;
    CHECK(opts.command == Options::Command::Test, "Command::Test is set");

    opts.command = Options::Command::Deps;
    CHECK(opts.command == Options::Command::Deps, "Command::Deps is set");

    opts.command = Options::Command::Docs;
    CHECK(opts.command == Options::Command::Docs, "Command::Docs is set");
}

static void test_build_derives_codegen_stage() {
    memory::Arena arena;

    Options plain(arena);
    plain.command = Options::Command::Build;
    plain.deriveTargetStage();
    CHECK(plain.targetStage == session::Stage::Cached,
          "build with no --emit reaches codegen (Stage::Cached)");

    Options emitIr(arena);
    emitIr.command    = Options::Command::Build;
    emitIr.emitTarget = Options::EmitTarget::Ir;
    emitIr.deriveTargetStage();
    CHECK(emitIr.targetStage == session::Stage::CodegenReady,
          "build --emit ir still stops at Stage::CodegenReady");

    Options emitVir(arena);
    emitVir.command = Options::Command::Build;
    emitVir.flags.emitVir(true);
    emitVir.deriveTargetStage();
    CHECK(emitVir.targetStage == session::Stage::HirLowered,
          "build --emit-vir stops at Stage::HirLowered");

    Options emitCst(arena);
    emitCst.command = Options::Command::Build;
    emitCst.flags.emitCst(true);
    emitCst.deriveTargetStage();
    CHECK(emitCst.targetStage == session::Stage::Imported,
          "build --emit-cst stops after frontend import");
}

static void test_new_emit_flags_parse_and_compose() {
    char program[] = "zithc";
    char command[] = "check";
    char cst[]     = "--emit-cst";
    char vir[]     = "--emit-vir";
    char all[]     = "--emit-all";
    char *args[]   = {program, command, cst, vir};
    Cli cli;
    cli.parseArgs(4, args);
    CHECK(cli.opts.flags.emitCst(), "--emit-cst enables CST emission");
    CHECK(cli.opts.flags.emitVir(), "--emit-vir enables VIR emission");

    char *all_args[] = {program, command, all};
    Cli all_cli;
    all_cli.parseArgs(3, all_args);
    CHECK(all_cli.opts.flags.emitCst() && all_cli.opts.flags.emitVir(),
          "--emit-all includes CST and VIR");
}

static void test_run_emit_vir_still_executes_program() {
#ifdef ZITHC_BINARY
    CliCapture capture;
    std::filesystem::create_directories(capture.root);
    const auto source = capture.root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "fn main(): i32 { return 7; }\n";
    }

#ifdef ZITH_HAS_VM
    const auto result = capture.run("run --emit-vir --no-cache \"" + source.string() + "\"");
    CHECK_EQ(result.exitCode, 7, "run --emit-vir executes main and returns its exit code");
    CHECK(result.stdoutText.empty(), "compiler VIR dump does not contaminate program stdout");
    CHECK(result.stderrText.find("--- VIR ---") != std::string::npos,
          "run --emit-vir writes the VIR dump to compiler stderr");
    CHECK(result.stderrText.find("LoadConstI32") != std::string::npos,
          "run --emit-vir includes lowered VM v2 instructions");
#else
    // The VM v2 slice is optional on native builds; without it --emit-vir is a
    // hard error, so the execution check is skipped.
    CHECK(true, "run --emit-vir execution is skipped when the VM slice is not built");
#endif
#else
    CHECK(true, "CLI subprocess tests are skipped when zithc is not built");
#endif
}

static void test_docs_options_parse_and_validate() {
    {
        char program[] = "zithc";
        char command[] = "docs";
        char *args[]   = {program, command};
        Cli cli;
        cli.parseArgs(2, args);
        CHECK(cli.opts.docsMode == Options::DocsMode::Interface, "docs defaults to interface mode");
        CHECK(cli.opts.docsOptionsError().empty(), "default docs options are valid");
    }

    {
        char program[] = "zithc";
        char command[] = "docs";
        char spec[]    = "--spec";
        char *args[]   = {program, command, spec};
        Cli cli;
        cli.parseArgs(3, args);
        CHECK(cli.opts.docsMode == Options::DocsMode::Spec, "--spec selects spec mode");
        CHECK(cli.opts.docsModeExplicit, "--spec records an explicit mode");
    }

    {
        char program[] = "zithc";
        char command[] = "docs";
        char mode[]    = "--mode=interface";
        char *args[]   = {program, command, mode};
        Cli cli;
        cli.parseArgs(3, args);
        CHECK(cli.opts.docsMode == Options::DocsMode::Interface,
              "--mode=interface selects interface mode");
        CHECK(cli.opts.docsModeExplicit, "--mode=interface records an explicit mode");
    }

    {
        char program[] = "zithc";
        char command[] = "docs";
        char out[]     = "--out";
        char index[]   = "--index";
        char *args[]   = {program, command, out, index};
        Cli cli;
        cli.parseArgs(4, args);
        CHECK(cli.opts.docsOut, "--out enables file output");
        CHECK(cli.opts.docsIndex, "--index enables multipage output");
        CHECK(cli.opts.docsOptionsError().empty(), "--index is valid with --out");
    }

    {
        char program[] = "zithc";
        char command[] = "docs";
        char out[]     = "--out=generated";
        char *args[]   = {program, command, out};
        Cli cli;
        cli.parseArgs(3, args);
        CHECK(cli.opts.docsOut, "--out=PATH enables file output");
        CHECK_EQ(cli.opts.docsOutPath, std::string("generated"),
                 "--out=PATH preserves the requested directory");
    }

    {
        char program[] = "zithc";
        char command[] = "docs";
        char spec[]    = "--spec";
        char iface[]   = "--interface";
        char *args[]   = {program, command, spec, iface};
        Cli cli;
        cli.parseArgs(4, args);
        CHECK(!cli.opts.docsOptionsError().empty(),
              "contradictory explicit docs modes are rejected");
    }

    {
        char program[] = "zithc";
        char command[] = "docs";
        char index[]   = "--index";
        char *args[]   = {program, command, index};
        Cli cli;
        cli.parseArgs(3, args);
        CHECK(!cli.opts.docsOptionsError().empty(), "--index without --out is rejected");
    }

    {
        char program[] = "zithc";
        char command[] = "docs";
        char mode[]    = "--mode=unknown";
        char *args[]   = {program, command, mode};
        Cli cli;
        cli.parseArgs(3, args);
        CHECK(!cli.opts.docsOptionsError().empty(), "unknown docs modes are rejected");
    }

    {
        char program[] = "zithc";
        char command[] = "build";
        char mode[]    = "--mode";
        char debug[]   = "debug";
        char *args[]   = {program, command, mode, debug};
        Cli cli;
        cli.parseArgs(4, args);
        CHECK(cli.opts.flags.mode() == Options::Mode::Debug,
              "the existing --mode debug build option is unchanged");
    }
}

static void test_docs_cli_rejects_invalid_options_before_compilation() {
#ifdef ZITHC_BINARY
    {
        CliCapture capture;
        const std::string result = capture.run("docs --mode=unknown", "stderr");
        CHECK(result.starts_with("1\n"), "unknown docs mode fails with exit code 1");
        CHECK(result.find("invalid docs mode") != std::string::npos,
              "unknown docs mode reports its argument error");
    }

    {
        CliCapture capture;
        const std::string result = capture.run("docs --out=first --out=second", "stderr");
        CHECK(result.starts_with("1\n"), "conflicting docs output paths fail with exit code 1");
        CHECK(result.find("conflicting --out paths") != std::string::npos,
              "conflicting docs output paths are reported");
    }

    {
        CliCapture capture;
        const std::string result = capture.run("build --mode=release", "stderr");
        CHECK(result.starts_with("1\n"), "unsupported global --mode= syntax remains an error");
        CHECK(result.find("unknown flag '--mode=release'") != std::string::npos,
              "global --mode=release is not consumed as a docs mode");
    }
#else
    CHECK(true, "CLI subprocess tests are skipped when zithc is not built");
#endif
}

static void test_completion_command_emits_updated_shell_options() {
    {
        char program[] = "zithc";
        char command[] = "completion";
        char shell[]   = "fish";
        char *args[]   = {program, command, shell};
        Cli cli;
        cli.parseArgs(3, args);
        CHECK(cli.opts.command == Options::Command::Completion,
              "completion is parsed as a CLI command");
        CHECK_EQ(cli.opts.subcommandStr, std::string("fish"),
                 "completion captures the requested shell");
    }

#ifdef ZITHC_BINARY
    CliCapture capture;
    for (const std::string shell : {"bash", "zsh", "fish"}) {
        const auto result = capture.run("completion " + shell);
        CHECK_EQ(result.exitCode, 0, "shell completion command succeeds");
        CHECK(result.stdoutText.starts_with("# " + shell + " completion for zithc"),
              "shell completion writes its script instead of the general help");

        const std::string optionPrefix = shell == "fish" ? "-l " : "--";
        CHECK(result.stdoutText.find(optionPrefix + "interface") != std::string::npos,
              "shell completion includes interface mode");
        CHECK(result.stdoutText.find(optionPrefix + "spec") != std::string::npos,
              "shell completion includes spec mode");
        CHECK(result.stdoutText.find(optionPrefix + "index") != std::string::npos,
              "shell completion includes the docs index option");
        CHECK(result.stdoutText.find(optionPrefix + "force") != std::string::npos,
              "shell completion includes the overwrite option");
    }
#else
    CHECK(true, "CLI completion subprocess test is skipped when zithc is not built");
#endif
}

static void test_docs_error_policy() {
    CliCapture capture;
    std::filesystem::create_directories(capture.root);
    const auto source = capture.root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "pub fn documented(): i32 { return 7; }\n"
                  "pub fn broken(): i32 { return missing(); }\n";
    }

    const auto defaultResult = capture.run("docs \"" + source.string() + "\"");
    CHECK_EQ(defaultResult.exitCode, 1,
             "docs reports a non-zero exit status when source compilation fails");
    CHECK(defaultResult.stdoutText.empty(),
          "docs without --error does not publish partial Markdown");

    const auto partialResult = capture.run("docs --error \"" + source.string() + "\"");
    CHECK_EQ(partialResult.exitCode, 1, "docs --error retains the failure exit status");
    CHECK(partialResult.stdoutText.find("documented") != std::string::npos,
          "docs --error includes successfully documented declarations");
    CHECK(partialResult.stdoutText.find("## Errors") != std::string::npos,
          "docs --error appends an Errors section");
    CHECK(partialResult.stdoutText.find("Functions") != std::string::npos,
          "docs --error identifies the section containing the diagnostic");

    const auto partialDirectory  = capture.root / "partial-output";
    const auto partialFileResult = capture.run("docs --error --out=\"" + partialDirectory.string() +
                                               "\" \"" + source.string() + "\"");
    CHECK_EQ(partialFileResult.exitCode, 1, "docs --error --out retains the failure exit status");
    CHECK(partialFileResult.stdoutText.empty(),
          "partial file output does not also publish Markdown on stdout");
    std::ifstream partialFile(partialDirectory / "API.md", std::ios::binary);
    const std::string partialText((std::istreambuf_iterator<char>(partialFile)),
                                  std::istreambuf_iterator<char>());
    CHECK(partialText.find("documented") != std::string::npos,
          "partial file output includes valid declarations");
    CHECK(partialText.find("## Errors") != std::string::npos,
          "partial file output includes the Errors section");
}

static void test_docs_parse_and_import_error_policy() {
#ifdef ZITHC_BINARY
    CliCapture capture;
    std::filesystem::create_directories(capture.root);

    struct ErrorCase {
        std::string name;
        std::string sourceText;
        std::string expectedSection;
    };
    const std::vector<ErrorCase> cases = {
        {"parse",
         "pub fn documented(): i32 { return 7; }\n"
         "pub fn broken(: i32 { return 0; }\n",
         "Functions"},
        {"import",
         "from missing_docs_dependency\n"
         "pub fn documented(): i32 { return 7; }\n",
         "Imports"},
    };

    for (const auto &errorCase : cases) {
        const auto source = capture.root / (errorCase.name + ".zith");
        {
            std::ofstream output(source, std::ios::binary | std::ios::trunc);
            output << errorCase.sourceText;
        }

        const auto outputDirectory = capture.root / (errorCase.name + "-output");
        const auto defaultResult   = capture.run("docs --out=\"" + outputDirectory.string() +
                                                 "\" \"" + source.string() + "\"");
        CHECK_EQ(defaultResult.exitCode, 1, "docs returns failure when a frontend error occurs");
        CHECK(defaultResult.stdoutText.empty(),
              "docs without --error suppresses partial stdout after a frontend error");
        CHECK(!std::filesystem::exists(outputDirectory / "API.md"),
              "docs without --error does not write API.md after a frontend error");

        const auto partialResult = capture.run("docs --error \"" + source.string() + "\"");
        CHECK_EQ(partialResult.exitCode, 1, "docs --error retains frontend failure status");
        CHECK(partialResult.stdoutText.find("documented") != std::string::npos,
              "docs --error retains valid declarations after a frontend error");
        CHECK(partialResult.stdoutText.find("## Errors") != std::string::npos,
              "docs --error renders frontend diagnostics");
        CHECK(partialResult.stdoutText.find(errorCase.expectedSection) != std::string::npos,
              "docs --error identifies the section containing the frontend error");
    }
#else
    CHECK(true, "CLI docs parse/import subprocess tests are skipped when zithc is not built");
#endif
}

static void test_docs_stdout_file_and_index_outputs() {
#ifdef ZITHC_BINARY
    CliCapture capture;
    std::filesystem::create_directories(capture.root);
    const auto source = capture.root / "main.zith";
    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << "pub fn documented(): i32 { return 7; }\n";
    }

    const auto terminalResult = capture.run("docs \"" + source.string() + "\"");
    CHECK_EQ(terminalResult.exitCode, 0, "docs succeeds for a valid standalone source file");
    CHECK(terminalResult.stdoutText.starts_with("# API Reference\n"),
          "docs writes the rendered Markdown to stdout by default");
    CHECK(terminalResult.stdoutText.find("documented") != std::string::npos,
          "stdout Markdown contains the public function");

    const auto outputDirectory = capture.root / "generated";
    const auto fileResult =
        capture.run("docs --out=\"" + outputDirectory.string() + "\" \"" + source.string() + "\"");
    CHECK_EQ(fileResult.exitCode, 0, "docs --out writes the aggregate document");
    CHECK(fileResult.stdoutText.empty(), "file output does not duplicate Markdown on stdout");
    std::ifstream apiFile(outputDirectory / "API.md", std::ios::binary);
    const std::string apiText((std::istreambuf_iterator<char>(apiFile)),
                              std::istreambuf_iterator<char>());
    CHECK(apiText.starts_with("# API Reference\n"), "--out creates API.md");

    const auto indexDirectory = capture.root / "indexed";
    const auto indexResult    = capture.run("docs --out=\"" + indexDirectory.string() +
                                            "\" --index \"" + source.string() + "\"");
    CHECK_EQ(indexResult.exitCode, 0, "--index writes the multipage documentation set");
    CHECK(std::filesystem::exists(indexDirectory / "README.md"), "--index creates a README index");
    const auto modulesDirectory = indexDirectory / "modules";
    CHECK(std::filesystem::is_directory(modulesDirectory),
          "--index creates the module pages directory");
    bool hasModulePage = false;
    for (const auto &entry : std::filesystem::directory_iterator(modulesDirectory))
        hasModulePage = hasModulePage || entry.path().extension() == ".md";
    CHECK(hasModulePage, "--index creates at least one Markdown module page");
#else
    CHECK(true, "CLI docs subprocess tests are skipped when zithc is not built");
#endif
}

// ── Command function signatures exist ─────────────────────────────

static void test_command_signatures_exist() {
    // Verify each contract signature compiles (pointer-to-function type check)
    using TestFn = int (*)(const Options &);
    TestFn t     = test;
    TestFn d     = deps;
    TestFn dc    = docs;
    // repl stays stub but must exist
    TestFn r = repl;
    (void)t;
    (void)d;
    (void)dc;
    (void)r;
    CHECK(true, "command function pointers resolve");
}

// ── Shared helpers ────────────────────────────────────────────────

static void test_count_passed() {
    std::vector<bool> all_pass = {true, true, true, true};
    CHECK(countPassed(all_pass) == 4, "countPassed returns 4 for all true");

    std::vector<bool> mixed = {true, false, true, false, false};
    CHECK(countPassed(mixed) == 2, "countPassed returns 2 for mixed");

    std::vector<bool> none_pass = {false, false};
    CHECK(countPassed(none_pass) == 0, "countPassed returns 0 for all false");

    std::vector<bool> empty;
    CHECK(countPassed(empty) == 0, "countPassed returns 0 for empty");
}

// ── Command names required by the contract ────────────────────────

static void test_command_names_match_contract() {
    memory::Arena arena;
    Options opts(arena);
    memory::StringInterner pool(arena);

    // All commands declared in commands.hpp must be callable
    int (*cmds[])(const Options &) = {
        check, build, execute, run, test, fmt, docs, repl, create, clean, deps, completion,
    };
    (void)cmds;

    // version and help have distinct signatures
    int v = version();
    (void)v;
    CHECK(true, "version() compiles");
}

static void test_llvm_version_information() {
#ifdef ZITH_LLVM_VERSION
    CHECK(std::strlen(ZITH_LLVM_VERSION) > 0,
          "LLVM version macro is non-empty when LLVM is present");
#endif
}

static void test_system_includes_flag() {
    memory::Arena arena;
    Options opts(arena);
    CHECK(opts.systemIncludes, "system includes default to enabled");

    char program[]    = "zithc";
    char command[]    = "check";
    char input[]      = "main.zith";
    char no_sysinc[]  = "--no-system-includes";
    char *without[]   = {program, command, input};
    char *with_flag[] = {program, command, no_sysinc, input};

    Cli plain;
    plain.parseArgs(3, without);
    CHECK(plain.opts.systemIncludes, "absent flag leaves system includes enabled");

    Cli disabled;
    disabled.parseArgs(4, with_flag);
    CHECK(!disabled.opts.systemIncludes, "--no-system-includes clears system includes");
}

static void test_debug_sema_flag() {
    memory::Arena arena;
    Options opts(arena);
    CHECK(!opts.flags.debugSema(), "debug-sema is off by default");

    char program[] = "zithc";
    char command[] = "check";
    char input[]   = "main.zith";
    char flag[]    = "--debug-sema";
    char *argv[]   = {program, command, flag, input};

    Cli cli;
    cli.parseArgs(4, argv);
    CHECK(cli.opts.flags.debugSema(), "--debug-sema enables the sema probe flag");
}

// ── ProjectConfig + Options merge helper ──────────────────────────

static void test_merge_strings_order_and_append() {
    memory::Arena arena;
    ProjectConfig config(arena);
    Options opts(arena);

    config.includeDirs.push("config/include");
    config.cSourceDirs.push("config/csrc");
    config.defines.push("CONFIG_DEFINE");
    config.libraryDirs.push("config/lib");
    config.libraries.push("config-lib");

    opts.includeDirs.push("cli/include");
    opts.cSourceDirs.push("cli/csrc");
    opts.defines.push("CLI_DEFINE");
    opts.libraryDirs.push("cli/lib");
    opts.libraries.push("cli-lib");

    std::vector<std::string> includeDirs;
    session::mergeStrings(config, opts, "includeDirs", includeDirs);
    CHECK(includeDirs.size() == 2, "includeDirs merge concatenates both sources");
    CHECK(includeDirs[0] == "config/include", "includeDirs keeps config order first");
    CHECK(includeDirs[1] == "cli/include", "includeDirs keeps CLI order second");

    std::vector<std::string> cSourceDirs;
    session::mergeStrings(config, opts, "cSourceDirs", cSourceDirs);
    CHECK(cSourceDirs.size() == 2 && cSourceDirs[0] == "config/csrc" &&
              cSourceDirs[1] == "cli/csrc",
          "cSourceDirs merge keeps config then CLI order");

    std::vector<std::string> defines;
    session::mergeStrings(config, opts, "defines", defines);
    CHECK(defines.size() == 2 && defines[0] == "CONFIG_DEFINE" && defines[1] == "CLI_DEFINE",
          "defines merge keeps config then CLI order");

    std::vector<std::string> libraryDirs;
    session::mergeStrings(config, opts, "libraryDirs", libraryDirs);
    CHECK(libraryDirs.size() == 2 && libraryDirs[0] == "config/lib" && libraryDirs[1] == "cli/lib",
          "libraryDirs merge keeps config then CLI order");

    std::vector<std::string> libraries;
    session::mergeStrings(config, opts, "libraries", libraries);
    CHECK(libraries.size() == 2 && libraries[0] == "config-lib" && libraries[1] == "cli-lib",
          "libraries merge keeps config then CLI order");

    std::vector<std::string> second;
    second.push_back("existing");
    session::mergeStrings(config, opts, "defines", second, /*append=*/false);
    CHECK(second.size() == 2 && second[0] == "CONFIG_DEFINE" && second[1] == "CLI_DEFINE",
          "mergeStrings append=false clears the existing output first");
}

static void test_merge_strings_source_filter() {
    memory::Arena arena;
    ProjectConfig config(arena);
    Options opts(arena);

    config.includeDirs.push("config/include");
    opts.cSourceDirs.push("cli/csrc");

    std::vector<std::pair<bool, std::string>> merged;
    session::mergeStrings(
        config, opts, "includeDirs",
        [&](const std::string &value, const bool fromCli) { merged.emplace_back(fromCli, value); });
    CHECK(merged.size() == 1 && !merged[0].first && merged[0].second == "config/include",
          "source-filter appender sees only the requested includeDirs");

    merged.clear();
    session::mergeStrings(
        config, opts, "cSourceDirs",
        [&](const std::string &value, const bool fromCli) { merged.emplace_back(fromCli, value); });
    CHECK(merged.size() == 1 && merged[0].first && merged[0].second == "cli/csrc",
          "source-filter appender sees only the requested cSourceDirs");
}

// ── All test aggregation ──────────────────────────────────────────

static void test_cli_commands() {
    test_options_defaults();
    test_options_command_enum();
    test_build_derives_codegen_stage();
    test_new_emit_flags_parse_and_compose();
    test_run_emit_vir_still_executes_program();
    test_docs_options_parse_and_validate();
    test_docs_cli_rejects_invalid_options_before_compilation();
    test_completion_command_emits_updated_shell_options();
    test_docs_error_policy();
    test_docs_parse_and_import_error_policy();
    test_docs_stdout_file_and_index_outputs();
    test_command_signatures_exist();
    test_count_passed();
    test_command_names_match_contract();
    test_llvm_version_information();
    test_system_includes_flag();
    test_debug_sema_flag();
    test_merge_strings_order_and_append();
    test_merge_strings_source_filter();
}

} // namespace

TEST_MAIN(cli_commands)
