// zith_cli.cpp — CLI entry point for the Zith compiler
//
// Requires C++17 (structured bindings, if-initializers).
// Depends on: CLI11, Zith/zith.h
#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include "CLI/App.hpp"
#include "CLI/ExtraValidators.hpp"
#include "cmd/commands.hpp"
#include "pipeline/pipeline.hpp"
#include "project_config/project_config.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <zith/zith.hpp>

using namespace zith::cli::commands;
using namespace zith::cli::pipeline;
using namespace zith::cli::project_config;

#ifdef _WIN32
static void enable_virtual_terminal_processing() {
    if (HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE); hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hErr, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hErr, dwMode);
        }
    }
}
#endif

extern "C" inline int zith_run(const int argc, const char *const argv[]) {
#ifdef _WIN32
    enable_virtual_terminal_processing();
#endif
    CLI::App app{"Zith - A low-level general-purpose language"};
    app.require_subcommand(0, 1);

    std::string mode_str = "debug";
    std::string output_file;
    std::vector<std::string> include_dirs;
    std::string emit_target;
    std::string target_triple;
    bool verbose = false;

    app.add_option("-m,--mode", mode_str, "Build mode: debug, dev, release, fast, test")
        ->transform(CLI::IsMember({"debug", "dev", "release", "fast", "test"}))
        ->default_str("debug");
    app.add_option("-o,--output", output_file, "Output file path");
    app.add_option("-I,--include", include_dirs, "Include directories (repeatable)");
    app.add_option("--emit", emit_target, "Emit: ast, ir, asm, obj, bin");
    app.add_option("--target", target_triple, "Target triple");
    app.add_flag("-v,--verbose", verbose, "Verbose output");

    std::string input_file;
    bool interpreted = false;
    bool fmt_check = false;
    std::string docs_output = "docs";

    auto *check_cmd = app.add_subcommand("check", "Parse and type-check only");
    check_cmd->add_option("input", input_file, "Source file [optional, reads toml if omitted]")
        ->check(CLI::ExistingFile);

    auto *compile_cmd = app.add_subcommand("compile", "Compile to object/bytecode, no linking");
    compile_cmd->add_option("input", input_file, "Source file (.zith)")
        ->required()
        ->check(CLI::ExistingFile);
    compile_cmd->add_flag("--interpreted", interpreted, "Compile to bytecode instead of native");

    auto *build_cmd = app.add_subcommand("build", "Compile and link to native binary");
    build_cmd->add_option("input", input_file, "Source file [optional, reads toml if omitted]")
        ->check(CLI::ExistingFile);

    auto *execute_cmd = app.add_subcommand("execute", "Run existing binary or bytecode");
    execute_cmd->add_option("target", input_file,
                            "Binary or bytecode [optional, reads toml if omitted]");
    execute_cmd->add_flag("--interpreted", interpreted, "Run bytecode instead of native binary");

    auto *run_cmd = app.add_subcommand("run", "Build then execute");
    run_cmd->add_option("input", input_file, "Source file [optional, reads toml if omitted]")
        ->check(CLI::ExistingFile);
    run_cmd->add_flag("--interpreted", interpreted, "Compile to bytecode and run interpreted");

    auto *test_cmd = app.add_subcommand("test", "Run examples in source");
    test_cmd->add_option("input", input_file, "Source file (.zith)")->check(CLI::ExistingFile);

    auto *fmt_cmd = app.add_subcommand("fmt", "Format source code");
    fmt_cmd->add_option("input", input_file, "Source file or directory")->required();
    fmt_cmd->add_flag("--check", fmt_check, "Check formatting only, do not modify files");

    auto *docs_cmd = app.add_subcommand("docs", "Generate documentation");
    docs_cmd->add_option("input", input_file, "Source file (.zith)")->check(CLI::ExistingFile);
    docs_cmd->add_option("-o,--output", docs_output, "Output directory")->default_str("docs");

    auto *repl_cmd = app.add_subcommand("repl", "Start interactive REPL");
    auto *new_cmd = app.add_subcommand("new", "Create a new project");
    new_cmd->add_option("name", input_file, "Project name")->required();

    auto *clean_cmd = app.add_subcommand("clean", "Remove build artifacts");
    auto *version_cmd = app.add_subcommand("version", "Show version information");
    auto *help_cmd = app.add_subcommand("help", "Show help message");

    try {
        app.parse(argc, argv);
    } catch (const CLI::CallForHelp &) {
        return cmd_help();
    } catch (const CLI::CallForVersion &) {
        return cmd_version();
    } catch (const CLI::ParseError &e) {
        if (e.get_name() == "ExtrasError" && argc > 1 && std::string(argv[1]) == "help")
            return cmd_help();
        std::cerr << "[error] " << e.what() << "\n\n" << app.help();
        return 1;
    }

    if (*help_cmd)
        return cmd_help();
    if (*version_cmd)
        return cmd_version();
    if (*new_cmd)
        return cmd_new(input_file, verbose);
    if (*clean_cmd)
        return cmd_clean(verbose);
    if (*repl_cmd)
        return cmd_repl(verbose);
    if (*docs_cmd)
        return cmd_docs(input_file, docs_output, verbose);
    if (*fmt_cmd)
        return cmd_fmt(input_file, fmt_check, verbose);
    if (*test_cmd)
        return cmd_test(input_file, verbose);
    if (*check_cmd)
        return cmd_check(input_file, mode_str, verbose, include_dirs);
    if (*compile_cmd)
        return cmd_compile(input_file, output_file, mode_str, interpreted, verbose, include_dirs);
    if (*build_cmd)
        return cmd_build(input_file, output_file, mode_str, verbose, include_dirs);
    if (*execute_cmd)
        return cmd_execute(input_file, interpreted, verbose, include_dirs);
    if (*run_cmd)
        return cmd_run(input_file, output_file, mode_str, interpreted, verbose, include_dirs);

    if (ZithProject proj; try_load_project(proj))
        return cmd_build("", "", mode_str, verbose, include_dirs);

    return cmd_help();
}
