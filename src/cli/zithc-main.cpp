#include "cli/options.hpp"
// #include "options.hpp"

int main(const int argc, char **argv) {
    zith::Cli programm;
    programm.parseArgs(argc, argv);
    programm.loadFlags();
    programm.loadProject();
    return programm.dispatch();
}
