#pragma once

#include "cli/options.hpp"

namespace zith::cli {

class CompilerDriver {
public:
    static int run(const Options &opts);
};

} // namespace zith::cli
