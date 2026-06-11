#include "cli/compiler-driver.hpp"
#include "cli/options.hpp"
#include <cstdio>
#include <cstdlib>

void foo(){
	for(int i = 32; i <= 126; i++) printf("%c\n", i);
}

int main(int argc, char **argv) {
    /*auto opts = zith::cli::parseArgs(argc, argv);
    return zith::cli::CompilerDriver::run(opts);*/
	foo();
}
