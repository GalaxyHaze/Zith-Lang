#include "driver/compiler-driver.hpp"
#include "driver/options.hpp"
#include "frontend/lexer/lexer.hpp"
#include "frontend/source/source-map.hpp"
#include <cstdio>

int main(int argc, char **argv) {
    auto opts = zith::driver::parseArgs(argc, argv);
    zith::driver::CompilerDriver driver;

    if (opts.input_files.empty()) {
        std::fprintf(
                stderr,
                "usage: zith <file> [--tokens] [--emit-ast] [--emit-hir] [--emit-mir] [-o out]\n");
        return 1;
    }

    if ( auto tokens = zith::frontend::lexer::tokenize("simpleTest", "fn main(){}") )
        zith::frontend::lexer::printTokens(tokens.value());
    else
        std::printf("some error had happen");
    return driver.compileWithOptions(opts);
}
