#include "cli/commands.hpp"
#include "cli/terminal.hpp"

#include <cstdio>

#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)

namespace zith::cli::commands {

int version() {
    auto term = term::init();
    term::UsagePrinter p{stdout, term.coutOn};

    p.bold("Zith ");
    std::printf("- A minimal system programming language\n"
                "Version:  %s\n"
                "Compiler: %s\n",
                ZITH_VERSION,
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

int help(FILE *dest) {
    auto term = term::init();
    term::UsagePrinter p{dest, term.coutOn};

    p.bold("Zith ");
    std::fprintf(dest, "- A minimal system programming language\n\n");
    p.section("USAGE:");
    std::fprintf(dest, "    zithc [OPTIONS] <COMMAND> [ARGS]\n\n");
    p.section("COMMANDS:");
    p.green("  -h, --help");
    std::fprintf(dest, "   Show this help message\n");
    p.green("      --version");
    std::fprintf(dest, " Show version information\n\n");
    p.section("OPTIONS:");
    p.flag("-h, --help", "Show help");
    p.flag("    --version", "Show version");
    p.flag("-m, --mode <debug|dev|release|fast|small>", "Build mode [default: debug]");
    p.flag("-o, --output <FILE>", "Output file path");
    p.flag("-I, --include <DIR>", "Add include directory (repeatable)");
    p.flag("-A, --assets <DIR>", "Add asset directory (repeatable)");
    p.flag("    --check", "Check formatting without modifying");
    p.flag("-i, --in-place", "Format files in-place");
    p.flag("    --emit <ast|hir|ir|asm|obj|bin>", "Emit intermediate representation");
    p.flag("    --target <TRIPLE>", "Target triple");
    p.flag("    --emit-tokens", "Print and emit tokens");
    p.flag("    --emit-ast", "Emit AST");
    p.flag("    --emit-hir", "Emit HIR");
    p.flag("    --emit-ir", "Emit LLVM IR");
    p.flag("    --emit-asm", "Emit assembly");
    p.flag("    --interpreted", "Use bytecode path");
    p.flag("    --opt-level <0-3>", "Optimization level");
    p.flag("    --debug-level <0-3>", "Debug info level");
    p.flag("-s, --strict", "Apply stricter rules");
    p.flag("    --lto", "Enable LTO");
    p.flag("    --strip-debug", "Strip debug symbols");
    p.flag("-c, --color <auto|on|off>", "Color output [default: auto]");
    p.flag("-v, --verbose", "Verbose output");
    std::fprintf(dest, "\n");
    p.section("EXAMPLES:");
    std::fprintf(dest, "    zithc build\n"
                       "    zithc run main.zith -m release\n"
                       "    zithc run main.zith         # build + execute\n"
                       "    zithc execute main.zith     # execute a pre-compiled binary\n");
    return 0;
}

} // namespace zith::cli::commands
