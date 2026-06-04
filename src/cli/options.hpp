#pragma once

#include <string>
#include <vector>

namespace zith::driver {

    struct Options {
        std::vector<std::string> input_files;
        std::string output_file           = "a.out";
        bool emit_tokens                  = false;
        bool emit_ast                     = false;
        bool emit_hir                     = false;
        bool emit_mir                     = false;
        bool print_tokens                 = false;
    };

    Options parseArgs(int argc, char **argv);

} // namespace zith::driver
