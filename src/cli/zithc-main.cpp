#include "cli/compiler-driver.hpp"
#include "cli/options.hpp"
#include "cli/zith-flags.hpp"

#include <cstdio>
#include <cstdlib>



int main(int argc, char **argv) {
    auto opts = zith::cli::parseArgs(argc, argv);
    auto defaults = zith::cli::loadZithFlags();
    zith::cli::CompilerDriver::run(opts);
}
