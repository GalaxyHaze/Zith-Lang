// impl/cli/CLI.hpp — CLI entry point for the Zith compiler
// Uses arg.hpp instead of CLI11. C++17.
#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include "arg.hpp"
#include "cmd/commands.hpp"
#include "pipeline/pipeline.hpp"
#include "project_config/project-config.hpp"
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

    std::string mode_str = "debug";
    std::string output_file;
    std::vector<std::string> include_dirs;
    std::string emit_target;
    std::string target_triple;
    bool verbose = false;

    zith::arg::Parser app{"Zith - A low-level general-purpose language", argc, argv};

    app.add_option("--mode", "-m", "Build mode: debug, dev, release, fast, test", &mode_str);
    app.add_option("--output", "-o", "Output file path", &output_file);
    app.add_repeatable("--include", "-I", "Include directories", &include_dirs);
    app.add_option("--emit", "", "Emit: ast, ir, asm, obj, bin", &emit_target);
    app.add_option("--target", "", "Target triple", &target_triple);
    app.add_flag("--verbose", "-v", "Verbose output", &verbose);

    std::string input_file;
    bool interpreted = false;
    bool fmt_check = false;
    std::string docs_output = "docs";

    app.add_subcommand("check", "Parse and type-check only", [&]() {
        return cmd_check(input_file, mode_str, verbose, include_dirs);
    });
    app.add_subcommand("compile", "Compile to object/bytecode, no linking", [&]() {
        return cmd_compile(input_file, output_file, mode_str, interpreted, verbose, include_dirs);
    });
    app.add_subcommand("build", "Compile and link to native binary", [&]() {
        return cmd_build(input_file, output_file, mode_str, verbose, include_dirs);
    });
    app.add_subcommand("execute", "Run existing binary or bytecode", [&]() {
        return cmd_execute(input_file, interpreted, verbose, include_dirs);
    });
    app.add_subcommand("run", "Build then execute", [&]() {
        return cmd_run(input_file, output_file, mode_str, interpreted, verbose, include_dirs);
    });
    app.add_subcommand("test", "Run examples in source", [&]() {
        return cmd_test(input_file, verbose);
    });
    app.add_subcommand("fmt", "Format source code", [&]() {
        return cmd_fmt(input_file, fmt_check, verbose);
    });
    app.add_subcommand("docs", "Generate documentation", [&]() {
        return cmd_docs(input_file, docs_output, verbose);
    });
    app.add_subcommand("repl", "Start interactive REPL", [&]() {
        return cmd_repl(verbose);
    });
    app.add_subcommand("new", "Create a new project", [&]() {
        return cmd_new(input_file, verbose);
    });
    app.add_subcommand("clean", "Remove build artifacts", [&]() {
        return cmd_clean(verbose);
    });
    app.add_subcommand("version", "Show version information", [&]() {
        return cmd_version();
    });
    app.add_subcommand("help", "Show help message", [&]() {
        return cmd_help();
    });

    if (!app.parse()) {
        std::cerr << app.help();
        return 1;
    }

    if (app.is_subcommand("help"))
        return cmd_help();
    if (app.is_subcommand("version"))
        return cmd_version();
    if (app.is_subcommand("new"))
        return cmd_new(input_file, verbose);
    if (app.is_subcommand("clean"))
        return cmd_clean(verbose);
    if (app.is_subcommand("repl"))
        return cmd_repl(verbose);
    if (app.is_subcommand("docs"))
        return cmd_docs(input_file, docs_output, verbose);
    if (app.is_subcommand("fmt"))
        return cmd_fmt(input_file, fmt_check, verbose);
    if (app.is_subcommand("test"))
        return cmd_test(input_file, verbose);
    if (app.is_subcommand("check"))
        return cmd_check(input_file, mode_str, verbose, include_dirs);
    if (app.is_subcommand("compile"))
        return cmd_compile(input_file, output_file, mode_str, interpreted, verbose, include_dirs);
    if (app.is_subcommand("build"))
        return cmd_build(input_file, output_file, mode_str, verbose, include_dirs);
    if (app.is_subcommand("execute"))
        return cmd_execute(input_file, interpreted, verbose, include_dirs);
    if (app.is_subcommand("run"))
        return cmd_run(input_file, output_file, mode_str, interpreted, verbose, include_dirs);

    if (ZithProject proj; try_load_project(proj))
        return cmd_build("", "", mode_str, verbose, include_dirs);

    return cmd_help();
}
