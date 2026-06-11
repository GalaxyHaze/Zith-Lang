#include "cli/compiler-driver.hpp"
#include "cli/options.hpp"

int main(const int argc, char **argv) {
    const auto opts = zith::cli::parseArgs(argc, argv);
    return zith::cli::CompilerDriver::run(opts);
}
