#include "cli/commands.hpp"
#include "diagnostics/color.hpp"

#include <cstdio>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

namespace zith::cli::commands {

static bool useColor() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

#define C(c) (useColor() ? diagnostics::ansi::c.data() : "")
#define RST (useColor() ? diagnostics::ansi::reset.data() : "")

int cmd_version() {
    std::printf("%sZith%s Programming Language\n"
                "Version:  %s\n"
                "Compiler: %s\n",
                C(bold), RST, ZITH_VERSION,
#if defined(__clang__)
                __clang_version__
#elif defined(__GNUC__) || defined(__GNUG__)
                "GCC " STRINGIFY(__GNUC__) "." STRINGIFY(__GNUC_MINOR__) "." STRINGIFY(
                    __GNUC_PATCHLEVEL__)
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
        "    %s-m, --mode%s <debug|dev|release|fast|test> Build mode [default: debug]\n"
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
        "    %s-c, --color%s <auto|on|off>               Color output [default: auto]\n"
        "    %s-v, --verbose%s                           Verbose output\n"
        "\n"
        "%sEXAMPLES:%s\n"
        "    zithc build\n"
        "    zithc run main.zith -m release\n"
        "    zithc compile --interpreted main.zith -o main.nbc\n"
        "    zithc execute --interpreted\n",
        C(bold), RST, C(bold), RST, C(bold), RST, C(green), RST, C(green), RST, C(bold), RST,
        C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan),
        RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST,
        C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan), RST, C(cyan),
        RST, C(cyan), RST, C(bold), RST);
    return 0;
}

} // namespace zith::cli::commands
