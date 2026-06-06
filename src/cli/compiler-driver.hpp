#pragma once

#include "cli/compilation-session.hpp"
#include "cli/options.hpp"

namespace zith::cli {

class CompilerDriver {
public:
    int run(const Options &opts);
};

} // namespace zith::cli
