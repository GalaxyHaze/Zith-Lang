#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/hir/hir-module.hpp"

namespace zith::zir::hir {

    class HirVerifier {
        const HirModule &module_;
        diagnostics::DiagnosticEngine &diags_;

    public:
        HirVerifier(const HirModule &module, diagnostics::DiagnosticEngine &diags);

        bool verify();
    };

} // namespace zith::zir::hir
