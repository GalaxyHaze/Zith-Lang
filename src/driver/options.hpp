#pragma once

#include "backend/interface/backend-options.hpp"

#include <string>
#include <vector>

namespace zith::driver {

    struct Options {
        std::vector<std::string> input_files;
        std::string output_file           = "a.out";
        backend::interface_::OptLevel opt = backend::interface_::OptLevel::O0;
        bool emit_tokens                  = false;
        bool emit_ast                     = false;
        bool emit_hir                     = false;
        bool emit_mir                     = false;
        bool print_tokens                 = false;
    };

    Options parseArgs(int argc, char **argv);

} // namespace zith::driver
