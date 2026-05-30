#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "middleend/mir/mir-module.hpp"

namespace zith::middleend::mir {

    class MirVerifier {
        const MirModule& module_;
        diagnostics::engine::DiagnosticEngine& diags_;

    public:
        MirVerifier(const MirModule& module, diagnostics::engine::DiagnosticEngine& diags);

        bool verify();
    };

}
