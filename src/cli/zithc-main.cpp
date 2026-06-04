#include "cli/compiler-driver.hpp"
#include "cli/options.hpp"
#include "compiler-driver.hpp"
#include "lexer/lexer.hpp"
#include "options.hpp"
#include "parser/source-map.hpp"
#include <cstdio>

int main(int argc, char **argv) {
    auto opts = zith::cli::parseArgs(argc, argv);
    zith::cli::CompilerDriver driver;

    if (opts.input_files.empty()) {
        std::fprintf(
                stderr,
                "usage: zith <file> [--tokens] [--emit-ast] [--emit-hir] [--emit-mir] [-o out]\n");
        return 1;
    }

    if ( auto tokens = zith::lexer::tokenize("simpleTest", "fn main(){}") )
        zith::lexer::printTokens(tokens.value());
    else
        std::printf("some error had happen");
    return driver.compileWithOptions(opts);
}
