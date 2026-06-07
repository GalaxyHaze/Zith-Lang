#include "cli/compiler-driver.hpp"
#include "cli/options.hpp"
#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv) {
    auto opts = zith::cli::parseArgs(argc, argv);
    return zith::cli::CompilerDriver::run(opts);
}
