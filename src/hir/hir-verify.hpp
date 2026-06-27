#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"

namespace zith::hir {

class HirVerifier {
    const HirModule &module_;
    diagnostics::DiagnosticEngine &diags_;

public:
    HirVerifier(const HirModule &module, diagnostics::DiagnosticEngine &diags);

    bool verify();
};

} // namespace zith::hir
