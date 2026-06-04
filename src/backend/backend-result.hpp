#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace zith::backend::interface_ {

    struct BackendResult {
        bool success = false;
        std::vector<uint8_t> bytecode;
        std::string object_path;
        diagnostics::engine::DiagnosticEngine diagnostics;
    };

} // namespace zith::backend::interface_
