#include "driver/compiler-driver.hpp"
#include "driver/options.hpp"

int main(int argc, char **argv) {
    auto opts = zith::driver::parseArgs(argc, argv);
    zith::driver::CompilerDriver driver;

    if (opts.input_files.empty()) {
        std::fprintf(
                stderr,
                "usage: zith <file> [--tokens] [--emit-ast] [--emit-hir] [--emit-mir] [-o out]\n");
        return 1;
    }

    return driver.compileWithOptions(opts);
}
