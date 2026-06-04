#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/mir/mir-module.hpp"

namespace zith::middleend::mir {

    class MirVerifier {
        const MirModule &module_;
        diagnostics::engine::DiagnosticEngine &diags_;

    public:
        MirVerifier(const MirModule &module, diagnostics::engine::DiagnosticEngine &diags);

        bool verify();
    };

} // namespace zith::middleend::mir
