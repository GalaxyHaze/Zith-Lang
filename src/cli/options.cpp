#include "options.hpp"
#include "cli/zith-flags.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zith::cli {

void Options::deriveTargetStage() {
    if (emit_ast)
        target_stage = Stage::Parsed;
    else if (emit_hir)
        target_stage = Stage::HirLowered;
    else if (emit_mir || emit_ir || emit_asm)
        target_stage = Stage::MirLowered;
    else if (!emit_target.empty()) {
        if (emit_target == "ast")
            target_stage = Stage::Parsed;
        else if (emit_target == "hir")
            target_stage = Stage::HirLowered;
        else if (emit_target == "mir" || emit_target == "ir" || emit_target == "asm")
            target_stage = Stage::MirLowered;
        else if (emit_target == "obj" || emit_target == "bin")
            target_stage = Stage::ZirInterpreted;
    }
}

static void printUsage() {
    std::fprintf(stderr,
        "Zith - A low-level general-purpose language\n"
        "\n"
        "USAGE:\n"
        "    zith [OPTIONS] <COMMAND> [ARGS]\n"
        "\n"
        "COMMANDS:\n"
        "    -h, --help   Show this help message\n"
        "        --version Show version information\n"
        "\n"
        "OPTIONS:\n"
        "    -h, --help                              Show help\n"
        "        --version                           Show version\n"
        "    -m, --mode <debug|dev|release|fast|test> Build mode\n"
        "    -o, --output <FILE>                     Output file path\n"
        "    -I, --include <DIR>                     Add include directory (repeatable)\n"
        "        --emit <ast|hir|mir|ir|asm|obj|bin> Emit intermediate representation\n"
        "        --target <TRIPLE>                   Target triple\n"
        "        --tokens                            Print and emit tokens\n"
        "        --emit-ast                          Emit AST\n"
        "        --emit-hir                          Emit HIR\n"
        "        --emit-mir                          Emit MIR\n"
        "        --emit-ir                           Emit LLVM IR\n"
        "        --emit-asm                          Emit assembly\n"
        "        --interpreted                       Use bytecode path\n"
        "        --opt-level <0-3>                   Optimization level\n"
        "        --debug-level <0-3>                 Debug info level\n"
        "    -s, --strict                            Apply stricter rules\n"
        "        --lto                               Enable LTO\n"
        "        --strip-debug                       Strip debug symbols\n"
        "    -c, --color <auto|on|off>               Color output\n"
        "    -v, --verbose                           Verbose output\n"
    );
}

static bool isSubcommand(const char *arg) {
    static const char *cmds[] = {
        "build", "run", "check", "compile", "execute",
        "test", "fmt", "docs", "repl",         "new", "clean", "deps",
        "version", "help", nullptr
    };
    for (const char **c = cmds; *c; ++c) {
        if (std::strcmp(arg, *c) == 0)
            return true;
    }
    return false;
}

static Options::Command subcommandToEnum(const char *arg) {
    if (std::strcmp(arg, "build") == 0)    return Options::Command::Build;
    if (std::strcmp(arg, "run") == 0)      return Options::Command::Run;
    if (std::strcmp(arg, "check") == 0)    return Options::Command::Check;
    if (std::strcmp(arg, "compile") == 0)  return Options::Command::Compile;
    if (std::strcmp(arg, "execute") == 0)  return Options::Command::Execute;
    if (std::strcmp(arg, "test") == 0)     return Options::Command::Test;
    if (std::strcmp(arg, "fmt") == 0)      return Options::Command::Fmt;
    if (std::strcmp(arg, "docs") == 0)     return Options::Command::Docs;
    if (std::strcmp(arg, "repl") == 0)     return Options::Command::Repl;
    if (std::strcmp(arg, "new") == 0)      return Options::Command::New;
    if (std::strcmp(arg, "clean") == 0)    return Options::Command::Clean;
    if (std::strcmp(arg, "deps") == 0)     return Options::Command::Deps;
    if (std::strcmp(arg, "version") == 0)  return Options::Command::Version;
    if (std::strcmp(arg, "help") == 0)     return Options::Command::Help;
    return Options::Command::None;
}

static void mergeFlags(zith::cli::Options &opts, const zith::cli::Options &defaults) {
    if (opts.output_file == "a.out" && defaults.output_file != "a.out")
        opts.output_file = defaults.output_file;
    if (opts.mode == "debug" && defaults.mode != "debug")
        opts.mode = defaults.mode;
    if (opts.target_triple.empty() && !defaults.target_triple.empty())
        opts.target_triple = defaults.target_triple;
    if (opts.opt_level == 0 && defaults.opt_level != 0)
        opts.opt_level = defaults.opt_level;
    if (opts.debug_level == 2 && defaults.debug_level != 2)
        opts.debug_level = defaults.debug_level;
    if (!opts.strict && defaults.strict)
        opts.strict = defaults.strict;
    if (!opts.lto && defaults.lto)
        opts.lto = defaults.lto;
    if (!opts.strip_debug && defaults.strip_debug)
        opts.strip_debug = defaults.strip_debug;
    if (opts.color == "auto" && defaults.color != "auto")
        opts.color = defaults.color;
    if (!opts.verbose && defaults.verbose)
        opts.verbose = defaults.verbose;
    if (opts.include_dirs.empty() && !defaults.include_dirs.empty()) {
        for (const auto &d : defaults.include_dirs)
            opts.include_dirs.push(d);
    }
    if (opts.emit_target.empty() && !defaults.emit_target.empty())
        opts.emit_target = defaults.emit_target;

    opts.deriveTargetStage();
}

Options parseArgs(int argc, char **argv) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        // Subcommand detection
        if (isSubcommand(argv[i]) && opts.command == Options::Command::None) {
            opts.command = subcommandToEnum(argv[i]);
            // Consume next arg as subcommand_arg for commands that take one
            if (opts.command == Options::Command::New ||
                opts.command == Options::Command::Clean ||
                opts.command == Options::Command::Deps) {
                if (i + 1 < argc && argv[i + 1][0] != '-')
                    opts.subcommand_arg = argv[++i];
            }
            continue;
        }

        // --help / -h
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            opts.command = Options::Command::Help;
            continue;
        }

        // --version
        if (std::strcmp(argv[i], "--version") == 0) {
            opts.command = Options::Command::Version;
            continue;
        }

        // --tokens
        if (std::strcmp(argv[i], "--tokens") == 0) {
            opts.emit_tokens = true;
            opts.print_tokens = true;
            continue;
        }

        // --emit-ast
        if (std::strcmp(argv[i], "--emit-ast") == 0) {
            opts.emit_ast = true;
            continue;
        }

        // --emit-hir
        if (std::strcmp(argv[i], "--emit-hir") == 0) {
            opts.emit_hir = true;
            continue;
        }

        // --emit-mir
        if (std::strcmp(argv[i], "--emit-mir") == 0) {
            opts.emit_mir = true;
            continue;
        }

        // --emit-ir
        if (std::strcmp(argv[i], "--emit-ir") == 0) {
            opts.emit_ir = true;
            continue;
        }

        // --emit-asm
        if (std::strcmp(argv[i], "--emit-asm") == 0) {
            opts.emit_asm = true;
            continue;
        }

        // --interpreted
        if (std::strcmp(argv[i], "--interpreted") == 0) {
            opts.interpreted = true;
            continue;
        }

        // --verbose / -v
        if (std::strcmp(argv[i], "--verbose") == 0 || std::strcmp(argv[i], "-v") == 0) {
            opts.verbose = true;
            continue;
        }

        // --strict / -s
        if (std::strcmp(argv[i], "--strict") == 0 || std::strcmp(argv[i], "-s") == 0) {
            opts.strict = true;
            continue;
        }

        // --lto
        if (std::strcmp(argv[i], "--lto") == 0) {
            opts.lto = true;
            continue;
        }

        // --strip-debug
        if (std::strcmp(argv[i], "--strip-debug") == 0) {
            opts.strip_debug = true;
            continue;
        }

        // --mode / -m
        if (std::strcmp(argv[i], "--mode") == 0 || std::strcmp(argv[i], "-m") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --mode/-m requires a value\n");
                printUsage();
                std::exit(1);
            }
            const char *val = argv[++i];
            if (std::strcmp(val, "debug") != 0 && std::strcmp(val, "dev") != 0 &&
                std::strcmp(val, "release") != 0 && std::strcmp(val, "fast") != 0 &&
                std::strcmp(val, "test") != 0) {
                std::fprintf(stderr, "error: invalid mode '%s' (expected debug|dev|release|fast|test)\n", val);
                std::exit(1);
            }
            opts.mode = val;
            continue;
        }

        // --output / -o
        if (std::strcmp(argv[i], "--output") == 0 || std::strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --output/-o requires a value\n");
                printUsage();
                std::exit(1);
            }
            opts.output_file = argv[++i];
            continue;
        }

        // --include / -I
        if (std::strcmp(argv[i], "--include") == 0 || std::strcmp(argv[i], "-I") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --include/-I requires a value\n");
                printUsage();
                std::exit(1);
            }
            const char *val = argv[++i];
            const char *start = val;
            for (const char *p = val; ; ++p) {
                if (*p == ',' || *p == '\0') {
                    if (p > start) {
                        opts.include_dirs.emplace(start, p - start);
                    }
                    if (*p == '\0')
                        break;
                    start = p + 1;
                }
            }
            continue;
        }

        // --emit
        if (std::strcmp(argv[i], "--emit") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --emit requires a value\n");
                printUsage();
                std::exit(1);
            }
            const char *val = argv[++i];
            if (std::strcmp(val, "ast") != 0 && std::strcmp(val, "hir") != 0 &&
                std::strcmp(val, "mir") != 0 && std::strcmp(val, "ir") != 0 &&
                std::strcmp(val, "asm") != 0 && std::strcmp(val, "obj") != 0 &&
                std::strcmp(val, "bin") != 0) {
                std::fprintf(stderr, "error: invalid emit target '%s' (expected ast|hir|mir|ir|asm|obj|bin)\n", val);
                std::exit(1);
            }
            opts.emit_target = val;
            continue;
        }

        // --target
        if (std::strcmp(argv[i], "--target") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --target requires a value\n");
                printUsage();
                std::exit(1);
            }
            opts.target_triple = argv[++i];
            continue;
        }

        // --opt-level
        if (std::strcmp(argv[i], "--opt-level") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --opt-level requires a value\n");
                printUsage();
                std::exit(1);
            }
            int val = std::atoi(argv[++i]);
            if (val < 0 || val > 3) {
                std::fprintf(stderr, "error: --opt-level must be 0-3\n");
                std::exit(1);
            }
            opts.opt_level = val;
            continue;
        }

        // --debug-level
        if (std::strcmp(argv[i], "--debug-level") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --debug-level requires a value\n");
                printUsage();
                std::exit(1);
            }
            int val = std::atoi(argv[++i]);
            if (val < 0 || val > 3) {
                std::fprintf(stderr, "error: --debug-level must be 0-3\n");
                std::exit(1);
            }
            opts.debug_level = val;
            continue;
        }

        // --color / -c
        if (std::strcmp(argv[i], "--color") == 0 || std::strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: --color/-c requires a value\n");
                printUsage();
                std::exit(1);
            }
            const char *val = argv[++i];
            if (std::strcmp(val, "auto") != 0 && std::strcmp(val, "on") != 0 &&
                std::strcmp(val, "off") != 0) {
                std::fprintf(stderr, "error: --color must be auto|on|off\n");
                std::exit(1);
            }
            opts.color = val;
            continue;
        }

        // Unknown flag
        if (argv[i][0] == '-') {
            std::fprintf(stderr, "error: unknown flag '%s'\n", argv[i]);
            printUsage();
            std::exit(1);
        }

        // Positional: input file
        opts.input_files.push(argv[i]);
    }

    opts.deriveTargetStage();
    mergeFlags(opts, loadZithFlags());
    return opts;
}

} // namespace zith::cli
