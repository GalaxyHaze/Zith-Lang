#pragma once

#include "driver/compilation-session.hpp"
#include "driver/options.hpp"

namespace zith::driver {

    class CompilerDriver {
    public:
        int compileFile(const std::string& path);
        int compileWithOptions(const Options& opts);
    };

}
