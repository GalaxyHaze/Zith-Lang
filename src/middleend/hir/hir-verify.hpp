#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "middleend/hir/hir-module.hpp"

namespace zith::middleend::hir {

    class HirVerifier {
        const HirModule& module_;
        diagnostics::engine::DiagnosticEngine& diags_;

    public:
        HirVerifier(const HirModule& module, diagnostics::engine::DiagnosticEngine& diags);

        bool verify();
    };

}
