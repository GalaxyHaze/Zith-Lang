#include "cli/commands.hpp"

#include <cstdio>

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x)      STRINGIFY_IMPL(x)

namespace zith::cli::commands {

int cmd_version() {
    std::printf("Zith Programming Language\n"
                "Version:  %s\n"
                "Compiler: %s\n",
                ZITH_VERSION,
#if defined(__clang__)
                __clang_version__
#elif defined(__GNUC__) || defined(__GNUG__)
                "GCC " STRINGIFY(__GNUC__) "." STRINGIFY(__GNUC_MINOR__) "." STRINGIFY(__GNUC_PATCHLEVEL__)
#elif defined(_MSC_FULL_VER)
                "MSVC " STRINGIFY(_MSC_FULL_VER)
#else
                "unknown compiler"
#endif
    );
    return 0;
}

int cmd_help() {
    std::printf(
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
        "    -m, --mode <debug|dev|release|fast|test> Build mode [default: debug]\n"
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
        "    -c, --color <auto|on|off>               Color output [default: auto]\n"
        "    -v, --verbose                           Verbose output\n"
        "\n"
        "EXAMPLES:\n"
        "    zith build\n"
        "    zith run main.zith -m release\n"
        "    zith compile --interpreted main.zith -o main.nbc\n"
        "    zith execute --interpreted\n"
    );
    return 0;
}

} // namespace zith::cli::commands
