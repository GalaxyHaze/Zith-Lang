#include "options.hpp"
#include "cli/zith-flags.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace zith::cli {

static bool useColor() {
#ifdef _WIN32
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

#define C(c) (useColor() ? diagnostics::ansi::c.data() : "")
#define RST (useColor() ? diagnostics::ansi::reset.data() : "")

void Options::deriveTargetStage() {
    if (emit_ast)
        target_stage = Stage::Imported;
    else if (emit_hir)
        target_stage = Stage::HirLowered;
    else if (emit_mir || emit_ir || emit_asm)
        target_stage = Stage::MirLowered;
    else if (!emit_target.empty()) {
        if (emit_target == "ast")
            target_stage = Stage::Imported;
        else if (emit_target == "hir")
            target_stage = Stage::HirLowered;
        else if (emit_target == "mir" || emit_target == "ir" || emit_target == "asm")
            target_stage = Stage::MirLowered;
        else if (emit_target == "obj" || emit_target == "bin")
            target_stage = Stage::ZirInterpreted;
    }
}

static void printUsage() {
    std::fprintf(
        stderr,
        "%sZith%s - A low-level general-purpose language\n"
        "\n"
        "%sUSAGE:%s\n"
        "    zithc [OPTIONS] <COMMAND> [ARGS]\n"
        "\n"
        "%sCOMMANDS:%s\n"
        "    %s-h, --help%s   Show this help message\n"
        "        %s--version%s Show version information\n"
        "\n"
        "%sOPTIONS:%s\n"
        "    %s-h, --help%s                              Show help\n"
        "        %s--version%s                           Show version\n"
        "    %s-m, --mode%s <debug|dev|release|fast|test> Build mode\n"
        "    %s-o, --output%s <FILE>                     Output file path\n"
        "    %s-I, --include%s <DIR>                     Add include directory (repeatable)\n"
        "        %s--emit%s <ast|hir|mir|ir|asm|obj|bin> Emit intermediate representation\n"
        "        %s--target%s <TRIPLE>                   Target triple\n"
        "        %s--tokens%s                            Print and emit tokens\n"
        "        %s--emit-ast%s                          Emit AST\n"
        "        %s--emit-hir%s                          Emit HIR\n"
        "        %s--emit-mir%s                          Emit MIR\n"
        "        %s--emit-ir%s                           Emit LLVM IR\n"
        "        %s--emit-asm%s                          Emit assembly\n"
        "        %s--interpreted%s                       Use bytecode path\n"
        "        %s--opt-level%s <0-3>                   Optimization level\n"
        "        %s--debug-level%s <0-3>                 Debug info level\n"
        "    %s-s, --strict%s                            Apply stricter rules\n"
        "        %s--lto%s                               Enable LTO\n"
        "        %s--strip-debug%s                       Strip debug symbols\n"
        "    %s-c, --color%s <auto|on|off>               Color output\n"
        "    %s-v, --verbose%s                           Verbose output\n",
        C(bold), RST, C(bold), RST, C(bold), RST, C(green), RST, C(green), RST, C(bold), RST,
        C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan),
        RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST,
        C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan),
        RST, C(cyan), RST);
}

static bool isSubcommand(const char *arg) {
    static const char *cmds[] = {"build", "run",  "check",   "compile", "execute",
                                 "test",  "fmt",  "docs",    "repl",    "new",
                                 "clean", "deps", "version", "help",    nullptr};
    for (const char **c = cmds; *c; ++c) {
        if (std::strcmp(arg, *c) == 0)
            return true;
    }
    return false;
}

static Options::Command subcommandToEnum(const char *arg) {
    if (std::strcmp(arg, "build") == 0)
        return Options::Command::Build;
    if (std::strcmp(arg, "run") == 0)
        return Options::Command::Run;
    if (std::strcmp(arg, "check") == 0)
        return Options::Command::Check;
    if (std::strcmp(arg, "compile") == 0)
        return Options::Command::Compile;
    if (std::strcmp(arg, "execute") == 0)
        return Options::Command::Execute;
    if (std::strcmp(arg, "test") == 0)
        return Options::Command::Test;
    if (std::strcmp(arg, "fmt") == 0)
        return Options::Command::Fmt;
    if (std::strcmp(arg, "docs") == 0)
        return Options::Command::Docs;
    if (std::strcmp(arg, "repl") == 0)
        return Options::Command::Repl;
    if (std::strcmp(arg, "new") == 0)
        return Options::Command::New;
    if (std::strcmp(arg, "clean") == 0)
        return Options::Command::Clean;
    if (std::strcmp(arg, "deps") == 0)
        return Options::Command::Deps;
    if (std::strcmp(arg, "version") == 0)
        return Options::Command::Version;
    if (std::strcmp(arg, "help") == 0)
        return Options::Command::Help;
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
            opts.include_dirs.push_back(d);
    }
    if (opts.emit_target.empty() && !defaults.emit_target.empty())
        opts.emit_target = defaults.emit_target;

    opts.deriveTargetStage();
}

void mergeProjectConfig(Options &opts, const ProjectConfig &cfg) {
    if (opts.mode == "debug" && !cfg.mode.empty() && cfg.mode != "debug")
        opts.mode = cfg.mode;
    if (opts.opt_level == 0 && cfg.opt_level != 0)
        opts.opt_level = cfg.opt_level;
    if (opts.output_file == "a.out" && !cfg.output.empty() && cfg.output != "a.out")
        opts.output_file = cfg.output;
}

Options parseArgs(int argc, char **argv) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        // Subcommand detection
        if (isSubcommand(argv[i]) && opts.command == Options::Command::None) {
            opts.command = subcommandToEnum(argv[i]);
            // Consume next arg as subcommand_arg for commands that take one
            if (opts.command == Options::Command::New || opts.command == Options::Command::Clean ||
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
            opts.emit_tokens  = true;
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
                std::fprintf(stderr, "%s[error]%s --mode/-m requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            const char *val = argv[++i];
            if (std::strcmp(val, "debug") != 0 && std::strcmp(val, "dev") != 0 &&
                std::strcmp(val, "release") != 0 && std::strcmp(val, "fast") != 0 &&
                std::strcmp(val, "test") != 0) {
                std::fprintf(stderr,
                             "%s[error]%s invalid mode '%s' (expected debug|dev|release|fast|test)\n",
                             C(red), RST, val);
                std::exit(1);
            }
            opts.mode = val;
            continue;
        }

        // --output / -o
        if (std::strcmp(argv[i], "--output") == 0 || std::strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s[error]%s --output/-o requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            opts.output_file = argv[++i];
            continue;
        }

        // --include / -I
        if (std::strcmp(argv[i], "--include") == 0 || std::strcmp(argv[i], "-I") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s[error]%s --include/-I requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            const char *val   = argv[++i];
            const char *start = val;
            for (const char *p = val;; ++p) {
                if (*p == ',' || *p == '\0') {
                    if (p > start) {
                        opts.include_dirs.emplace_back(start, static_cast<size_t>(p - start));
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
                std::fprintf(stderr, "%s[error]%s --emit requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            const char *val = argv[++i];
            if (std::strcmp(val, "ast") != 0 && std::strcmp(val, "hir") != 0 &&
                std::strcmp(val, "mir") != 0 && std::strcmp(val, "ir") != 0 &&
                std::strcmp(val, "asm") != 0 && std::strcmp(val, "obj") != 0 &&
                std::strcmp(val, "bin") != 0) {
                std::fprintf(
                    stderr,
                    "%s[error]%s invalid emit target '%s' (expected ast|hir|mir|ir|asm|obj|bin)\n",
                    C(red), RST, val);
                std::exit(1);
            }
            opts.emit_target = val;
            continue;
        }

        // --target
        if (std::strcmp(argv[i], "--target") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s[error]%s --target requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            opts.target_triple = argv[++i];
            continue;
        }

        // --opt-level
        if (std::strcmp(argv[i], "--opt-level") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s[error]%s --opt-level requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            int val = std::atoi(argv[++i]);
            if (val < 0 || val > 3) {
                std::fprintf(stderr, "%s[error]%s --opt-level must be 0-3\n", C(red), RST);
                std::exit(1);
            }
            opts.opt_level = val;
            continue;
        }

        // --debug-level
        if (std::strcmp(argv[i], "--debug-level") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s[error]%s --debug-level requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            int val = std::atoi(argv[++i]);
            if (val < 0 || val > 3) {
                std::fprintf(stderr, "%s[error]%s --debug-level must be 0-3\n", C(red), RST);
                std::exit(1);
            }
            opts.debug_level = val;
            continue;
        }

        // --color / -c
        if (std::strcmp(argv[i], "--color") == 0 || std::strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s[error]%s --color/-c requires a value\n", C(red), RST);
                printUsage();
                std::exit(1);
            }
            const char *val = argv[++i];
            if (std::strcmp(val, "auto") != 0 && std::strcmp(val, "on") != 0 &&
                std::strcmp(val, "off") != 0) {
                std::fprintf(stderr, "%s[error]%s --color must be auto|on|off\n", C(red), RST);
                std::exit(1);
            }
            opts.color = val;
            continue;
        }

        // Unknown flag
        if (argv[i][0] == '-') {
            std::fprintf(stderr, "%s[error]%s unknown flag '%s'\n", C(red), RST, argv[i]);
            printUsage();
            std::exit(1);
        }

        // Positional: input file
        opts.input_files.push_back(argv[i]);
    }

    opts.deriveTargetStage();
    mergeFlags(opts, loadZithFlags());
    return opts;
}

} // namespace zith::cli
