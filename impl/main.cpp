
#ifdef ZITH_WASM
#include <cassert>
#include <iostream>

int main() {
    std::cout << "Thank you for installing Zith." << std::endl;
    std::cout << "The WASM version is currently under development and opaque." << std::endl;
    std::cout << "I'm doing my best to bring you new features soon. Stay tuned!" << std::endl;
    return 0;
}
#else

#include <zith/zith.hpp>
#ifndef ZITH_WASM
#include <ffi.h>
#endif

int main(const int argc, const char *argv[]) {
    zith_run(argc, argv);
}

#endif