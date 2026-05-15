#include "commands.hpp"

#include <iostream>
#include <string>

#if defined(__VERSION__)
static std::string compiler = __VERSION__;
#elif defined(_MSC_FULL_VER)
static std::string compiler = "MSVC " + std::to_string(_MSC_FULL_VER);
#else
static std::string compiler = "unknown compiler";
#endif

namespace zith::cli::commands {

int cmd_version() {
    std::cout << "Zith Programming Language\n"
              << "Version:  " << ZITH_VERSION << "\n"
              << "Compiler: " << compiler << "\n";
    return 0;
}

int cmd_help() {
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

} // namespace zith::cli::commands
