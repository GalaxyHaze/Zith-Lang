// zith_cli.cpp — CLI entry point for the Zith compiler
//
// Requires C++17 (structured bindings, if-initializers).
// Depends on: CLI11, Zith/zith.h

#include "../ast/ast.h"
#include "../lexer/debug.h"
#include "pipeline/pipeline.hpp"
#include "project_config/project_config.hpp"
#include "runtime_interpreted/runtime_interpreted.hpp"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <zith/zith.hpp>
// #include "runtime_interpreted/runtime_interpreted.hpp"

static const char *zith_version = ZITH_VERSION;

// ============================================================================
// Output helpers
// ============================================================================

static void print_error(const std::string &msg) {
    std::cerr << "[error] " << msg << "\n";
}

static void print_info(const std::string &msg) {
    std::cout << "[*] " << msg << "\n";
}

static void print_success(const std::string &action, const std::string &target) {
    std::cout << "[ok] " << action << ": " << target << "\n";
}

static void print_not_implemented(const std::string &command) {
    std::cerr << "\n[!] Command '" << command << "' is not implemented yet.\n"
              << "    This feature will be available in a future version of Zith.\n"
              << "    Track progress at: https://github.com/GalaxyHaze/Zith\n\n";
}

// ============================================================================
// Pipeline
// ============================================================================

// Tokenizes 'src_path' into a new arena.
// Fills 'out_stream', 'out_source' and 'out_source_len' — the source stays in
// the arena to be reused by the parser without a second load.
// Returns the arena (caller destroys it); nullptr on error.
ZithArena *tokenize_file(const std::string &src_path, ZithTokenStream &out_stream,
                         const char **out_source, size_t *out_source_len, bool verbose) {
    ZithArena *arena = zith_arena_create(64 * 1024);
    if (!arena) {
        print_error("Failed to create memory arena");
        return nullptr;
    }

    size_t file_size   = 0;
    const char *source = zith_load_file_to_arena(arena, src_path.c_str(), &file_size);
    if (!source) {
        print_error("Failed to load file: " + src_path);
        zith_arena_destroy(arena);
        return nullptr;
    }

    out_stream = zith_tokenize(arena, source, file_size);
    if (!out_stream.data) {
        zith_arena_destroy(arena);
        return nullptr;
    }

    if (verbose)
        print_info("Tokenized " + std::to_string(out_stream.len) + " tokens from " + src_path);

    // Return source pointer — already in arena, no extra allocation needed
    if (out_source)
        *out_source = source;
    if (out_source_len)
        *out_source_len = file_size;

    return arena;
}

// ============================================================================
// Commands
// ============================================================================

// check — parse + semantics, only reports errors, produces no artifact
static int cmd_check(const std::string &input_file, const std::string &mode_str, bool verbose,
                     const std::vector<std::string> &include_dirs) {
    std::string src = input_file;

    if (src.empty()) {
        zith::cli::project_config::ZithProject proj;
        if (!zith::cli::project_config::try_load_project(proj)) {
            print_error("No input file and no ZithProject.toml found");
            return 1;
        }
        src = proj.entry;
    }

    if (verbose)
        print_info("Checking '" + src + "' in " + mode_str + " mode...");

    ZithTokenStream stream{};
    const char *source = nullptr;
    size_t src_size    = 0;
    ZithArena *arena = zith::cli::pipeline::tokenize_file(src, stream, &source, &src_size, verbose);
    if (!arena)
        return 1;

    zith_debug_tokens(stream.data, stream.len);

    std::vector<const char *> import_roots;
    size_t import_root_count;
    zith::cli::project_config::build_import_roots(include_dirs, import_roots, import_root_count);

    ZithNode *ast = zith_parse_with_source(arena, source, src_size, src.c_str(), stream,
                                           import_roots.data(), import_root_count);

    // Debug: AST dump
    if (verbose) {
        if (ast) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            printf("\n\n\n-Starting AST\n");
            zith_ast_print(ast, 0);
            printf("Ending AST\n\n\n");
        } else {
            print_error("Parse failed — null AST\n\n");
            zith_arena_destroy(arena);
            return 1;
        }
    }

    if (!ast) {
        zith_arena_destroy(arena);
        return 1;
    }

    zith_arena_destroy(arena);
    print_success("Check passed", src);
    return 0;
}

// compile — check + generates native object (.o) or bytecode (.nbc), no linking
static int cmd_compile(const std::string &input_file, const std::string &output_file,
                       const std::string &mode_str, bool interpreted, bool verbose,
                       const std::vector<std::string> &include_dirs) {
    (void)include_dirs;
    if (verbose) {
        const std::string kind = interpreted ? "bytecode" : "LLVM IR / native object";
        print_info("Compiling '" + input_file + "' → " + kind + " (" + mode_str + ")");
    }

    ZithTokenStream stream{};
    const char *source = nullptr;
    size_t src_size    = 0;
    ZithArena *arena =
        zith::cli::pipeline::tokenize_file(input_file, stream, &source, &src_size, verbose);
    if (!arena)
        return 1;

    std::vector<const char *> import_roots;
    size_t import_root_count;
    zith::cli::project_config::build_import_roots(include_dirs, import_roots, import_root_count);

    ZithNode *ast = zith_parse_with_source(arena, source, src_size, input_file.c_str(), stream,
                                           import_roots.data(), import_root_count);
    if (!ast) {
        zith_arena_destroy(arena);
        return 1;
    }

    // TODO [LLVM — native path]:
    //   - Initialize LLVMContext + LLVMModule + LLVMBuilder
    //   - Traverse AST and emit LLVMValueRef per node (codegen)
    //   - Apply optimization passes via LLVMPassManager (opt_level 0–3)
    //   - LLVMTargetMachineEmitToFile → .o  (or .ll if --emit ir)
    //   - If --emit asm → emit .s via LLVMAssemblyFile
    //   - Consider llvm::orc::LLJIT for future JIT (REPL)

    // TODO [Bytecode — interpreted path]:
    //   - Define .nbc format:
    //       header: magic + version + constant count + instruction count
    //       constant pool (strings, numbers)
    //       instruction array (1 byte opcode + variable operands)
    //   - zith_bytecode_emit(arena, ast) → ZithBytecode*
    //   - Serialize to .nbc file

    // TODO: use include_dirs to resolve imports in parse/sema

    const std::string out = !output_file.empty() ? output_file : (interpreted ? "a.zbc" : "a.o");
    if (interpreted) {
        std::ofstream ofs(out, std::ios::binary);
        if (!ofs) {
            print_error("Failed to write bytecode file: " + out);
            zith_arena_destroy(arena);
            return 1;
        }
        ofs << source;
    }
    zith_arena_destroy(arena);
    print_success(interpreted ? "Bytecode compile" : "Compile", out);
    return 0;
}

// build — compile (native) + link → final binary
static int cmd_build(const std::string &input_file, const std::string &output_file,
                     const std::string &mode_str, bool verbose,
                     const std::vector<std::string> &include_dirs) {
    std::string src                         = input_file;
    std::string out                         = output_file;
    std::string mode                        = mode_str;
    std::vector<std::string> extra_includes = include_dirs;

    if (src.empty()) {
        zith::cli::project_config::ZithProject proj;
        if (!zith::cli::project_config::try_load_project(proj)) {
            print_error("No input file and no ZithProject.toml found");
            return 1;
        }
        src = proj.entry;
        if (out.empty())
            out = proj.output;
        if (mode == "debug")
            mode = proj.mode;

        // Inherit project configurations
        extra_includes.insert(extra_includes.end(), proj.include_dirs.begin(),
                              proj.include_dirs.end());

        // TODO: propagar proj.lib_paths, proj.link_libs, proj.link_flags
        // TODO: aplicar proj.lto se mode == "release"
        // TODO: aplicar proj.opt_level / proj.debug_level ao LLVMTargetMachine
        // TODO: create proj.bin_dir and proj.cache_dir if they don't exist
    }

    if (verbose)
        print_info("Building '" + src + "' → binary (" + mode + ")");

    // compile: native only (interpreted = false)
    if (const int rc = cmd_compile(src, "", mode, /*interpreted=*/false, verbose, extra_includes);
        rc != 0)
        return rc;

    // TODO: invoke linker (embedded LLD or system linker as fallback)
    //   - collect all .o files (multi-file support in the future)
    //   - add project lib_paths and link_libs
    //   - produce final binary at `out`
    // TODO: strip debug if proj.strip_debug == true

    const std::string binary = out.empty() ? "a.out" : out;
    print_success("Build", binary);
    return 0;
}

// execute — runs an existing binary or bytecode
static int cmd_execute(const std::string &target, bool interpreted, bool verbose,
                       const std::vector<std::string> &include_dirs) {
    std::string bin = target;

    if (bin.empty()) {
        zith::cli::project_config::ZithProject proj;
        if (!zith::cli::project_config::try_load_project(proj)) {
            print_error("No target specified and no ZithProject.toml found");
            return 1;
        }
        bin = interpreted ? (proj.output + ".nbc") : proj.output;
    }

    if (!zith_file_exists(bin.c_str())) {
        const std::string hint = interpreted ? "compile --interpreted" : "build";
        print_error("Target not found: '" + bin + "' -- did you run 'zith " + hint + "' first?");
        return 1;
    }

    if (verbose)
        print_info("Executing '" + bin + "'" + (interpreted ? " (interpreted)" : ""));

    // TODO [native]: execv (POSIX) / CreateProcess (Windows)
    //   - pass remaining argv to child process
    //   - return child process exit code

    if (interpreted) {
        std::ifstream ifs(bin, std::ios::binary);
        if (!ifs) {
            print_error("Failed to read bytecode file: " + bin);
            return 1;
        }
        std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ZithArena *arena = zith_arena_create(64 * 1024);
        if (!arena)
            return 1;
        ZithTokenStream stream = zith_tokenize(arena, source.c_str(), source.size());
        if (!stream.data) {
            zith_arena_destroy(arena);
            return 1;
        }
        std::vector<const char *> import_roots;
        size_t import_root_count;
        zith::cli::project_config::build_import_roots(include_dirs, import_roots,
                                                      import_root_count);

        ZithNode *ast = zith_parse_with_source(arena, source.c_str(), source.size(), bin.c_str(),
                                               stream, import_roots.data(), import_root_count);
        if (!ast) {
            zith_arena_destroy(arena);
            return 1;
        }
        const int rc = zith::cli::runtime_interpreted::run_interpreted_ast(ast);
        zith_arena_destroy(arena);
        return rc;
    }
    print_not_implemented("execute");
    return 1;
}

// run — build/compile + execute in a single invocation
static int cmd_run(const std::string &input_file, const std::string &output_file,
                   const std::string &mode_str, bool interpreted, bool verbose,
                   const std::vector<std::string> &include_dirs) {
    if (interpreted) {
        const std::string bc = output_file.empty() ? "a.nbc" : output_file;
        if (const int rc = cmd_compile(input_file, bc, mode_str,
                                       /*interpreted=*/true, verbose, include_dirs);
            rc != 0)
            return rc;
        return cmd_execute(bc, /*interpreted=*/true, verbose, include_dirs);
    }

    if (const int rc = cmd_build(input_file, output_file, mode_str, verbose, include_dirs); rc != 0)
        return rc;

    const std::string binary = output_file.empty() ? "a.out" : output_file;
    return cmd_execute(binary, /*interpreted=*/false, verbose, include_dirs);
}

static int cmd_test(const std::string & /*input_file*/, bool /*verbose*/) {
    // TODO: test syntax: #[test] fn my_test() { ... }
    // TODO: auto-discover *_test.zith files in test_dir
    // TODO: compile and run each test in isolation
    // TODO: report: passed / failed / ignored with execution time
    // TODO: filter by name: zith test foo::bar
    print_not_implemented("test");
    return 1;
}

static int cmd_fmt(const std::string & /*input_file*/, bool /*check_only*/, bool /*verbose*/) {
    // TODO: pretty-printer over the AST (canonical form)
    // TODO: support single file or recursive directory
    // TODO: --check → does not modify, returns 1 if any file differs (CI)
    // TODO: .zith_fmt for style configuration
    print_not_implemented("fmt");
    return 1;
}

static int cmd_docs(const std::string & /*input_file*/, const std::string & /*output_dir*/,
                    bool /*verbose*/) {
    // TODO: extract doc-comments (/// or /** */) during parsing
    // TODO: generate static HTML or Markdown per module/function/type
    // TODO: support embedded examples in doc-comments (runnable snippets)
    print_not_implemented("docs");
    return 1;
}

static int cmd_repl(bool /*verbose*/) {
    // TODO: llvm::orc::LLJIT para JIT compilation
    // TODO: readline / linenoise — input with history
    // TODO: compilar cada linha/bloco incrementalmente
    // TODO: maintain variable context across expressions
    // TODO: comandos especiais: :quit, :type <expr>, :help, :load <file>
    print_not_implemented("repl");
    return 1;
}

static int cmd_new(const std::string &project_name, bool verbose) {
    namespace fs = std::filesystem;

    const fs::path root = fs::path(project_name).filename();
    if (fs::exists(root)) {
        print_error("Directory '" + root.string() + "' already exists");
        return 1;
    }

    // ── Create directories ────────────────────────────────────────────────
    fs::create_directories(root / "src");
    fs::create_directories(root / "asset");
    fs::create_directories(root / "docs");
    fs::create_directories(root / ".zmodules");
    fs::create_directories(root / "tests");

    if (verbose) {
        print_info("Created project structure: " + root.string());
    }

    // ── ZithProject.toml ──────────────────────────────────────────────────
    {
        std::ofstream toml(root / "ZithProject.toml");
        if (!toml) {
            print_error("Failed to create ZithProject.toml");
            return 1;
        }
        toml << "# ZithProject.toml — generated by 'zith new " << project_name << "'\n";
        toml << "\n";
        toml << "[project]\n";
        toml << "name = \"" << project_name << "\"\n";
        toml << "version = \"0.1.0\"\n";
        toml << "description = \"\"\n";
        toml << "authors = \"\"\n";
        toml << "license = \"MIT\"\n";
        toml << "homepage = \"\"\n";
        toml << "\n";
        toml << "[build]\n";
        toml << "entry = \"src/main.zith\"\n";
        toml << "output = \"bin/" << project_name << "\"\n";
        toml << "mode = \"debug\"\n";
        toml << "edition = \"2024\"\n";
        toml << "opt_level = 0\n";
        toml << "debug_level = 2\n";
        toml << "\n";
        toml << "[paths]\n";
        toml << "src_dir = \"src\"\n";
        toml << "bin_dir = \"target\"\n";
        toml << "mod_dir = \".zmodules\"\n";
        toml << "docs_dir = \"docs\"\n";
        toml << "test_dir = \"test\"\n";
        toml << "cache_dir = \".zcache\"\n";
        toml << "\n";
        toml << "# Optional: dependencies, link_libs, include_dirs, etc.\n";
    }

    // ── src/main.zith ─────────────────────────────────────────────────────
    {
        std::ofstream main_zith(root / "src" / "main.zt");
        if (!main_zith) {
            print_error("Failed to create src/main.zt");
            return 1;
        }
        main_zith << "// " << project_name << " — main entry point\n";
        main_zith << "// Generated by 'zith new " << project_name << "'\n";
        main_zith << "\nfrom std/io/console;\n\n";
        main_zith << "fn main() {\n";
        main_zith << "    // TODO: your code here\n";
        main_zith << "    println(\"Hello from " << project_name << "!\")\n";
        main_zith << "}\n";
    }

    print_success("Created project", root.string());
    if (verbose) {
        std::cout << "  cd " << root.string() << " && zith build\n";
    }
    return 0;
}

static int cmd_clean(bool /*verbose*/) {
    // TODO: remover bin_dir (bin/)
    // TODO: remover cache_dir (.zith_cache/)
    // TODO: remover ficheiros .o, .nbc, .ll, .s
    // TODO: --frozen option to remove everything
    print_not_implemented("clean");
    return 1;
}

#if defined(__VERSION__)
std::string compiler = __VERSION__;
#elif defined(_MSC_FULL_VER)
std::string compiler = "MSVC " + std::to_string(_MSC_FULL_VER);
#else
std::string compiler = "unknown compiler";
#endif

static int cmd_version() {
    std::cout << "Zith Programming Language\n"
              << "Version:  " << zith_version << "\n"
              << "Compiler: " << compiler << "\n";
    // TODO: LLVMGetVersion() quando o backend estiver linkado
    // TODO: LLVMGetDefaultTargetTriple() para mostrar o host target
    return 0;
}

static int cmd_help() {
    std::cout << R"(Zith - A low-level general-purpose language

USAGE:
    zith [OPTIONS] <COMMAND> [ARGS]

COMMANDS:
    build        Compile and link to native binary (reads ZithProject.toml)
    run          Build then execute (reads ZithProject.toml)
    check        Parse and type-check; report errors only, no output
    compile      Compile to object file or bytecode, no linking
    execute      Run an existing binary or bytecode (reads ZithProject.toml)
    test         Run examples in source
    repl         Start interactive REPL
    new          Create a new project
    fmt          Format source code
    docs         Generate documentation
    clean        Remove build artifacts
    version      Show version information
    help         Show this help message

OPTIONS:
    -m, --mode <debug|dev|release|fast|test>    Build mode [default: debug]
    -o, --output <FILE>                         Output file path
    -I, --include <DIR>                         Add include directory (repeatable)
        --interpreted                           Use bytecode path instead of native
        --emit <ast|ir|asm|obj|bin>             Emit intermediate representation
        --target <TRIPLE>                       Target triple (e.g. x86_64-linux-gnu)
    -s, --strict                                Apply stricter rules to the compiler
    -v, --verbose                               Use verbose output
    -c, --color <auto|on|off>                   Set color output [default: auto]
    -h, --help                                  Show help

PIPELINE:
    check  <  compile  <  build
    execute            <  run
    run --interpreted  =  compile --interpreted + execute --interpreted

EXAMPLES:
    zith build
    zith run main.zith -m release
    zith compile --interpreted main.zith -o main.nbc
    zith execute --interpreted

LEARN MORE:
    Source: https://github.com/GalaxyHaze/Zith
    Docs:   https://galaxyhaze.github.io/Zith/
    )";
    return 0;
}

// ============================================================================
// Entry point (C API)
// ============================================================================

extern "C" int zith_run(int argc, const char *const argv[]) {
    CLI::App app{"Zith - A low-level general-purpose language"};
    app.require_subcommand(0, 1);

    // Global options
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

    // TODO: propagate emit_target and target_triple to cmd_compile / cmd_build
    // TODO: validate target_triple against supported LLVM targets

    // Subcommands
    std::string input_file;
    bool interpreted        = false;
    bool fmt_check          = false;
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
    auto *new_cmd  = app.add_subcommand("new", "Create a new project");
    new_cmd->add_option("name", input_file, "Project name")->required();

    auto *clean_cmd   = app.add_subcommand("clean", "Remove build artifacts");
    auto *version_cmd = app.add_subcommand("version", "Show version information");
    auto *help_cmd    = app.add_subcommand("help", "Show help message");

    // -- Parse ----------------------------------------------------------------
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

    // -- Dispatch -------------------------------------------------------------
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

    // No subcommand: try build via toml, otherwise show help
    zith::cli::project_config::ZithProject proj;
    if (try_load_project(proj))
        return cmd_build("", "", mode_str, verbose, include_dirs);

    return cmd_help();
}
