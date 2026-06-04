#pragma once

#include "cli/compilation-session.hpp"
#include "cli/options.hpp"

namespace zith::driver {

    class CompilerDriver {
    public:
        int compileFile(const std::string &path);
        int compileWithOptions(const Options &opts);
    };

} // namespace zith::driver
