#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"

namespace zith::hir {

class HirVerifier {
    const HirModule &module_[[maybe_unused]];
    diagnostics::DiagnosticEngine &diags_[[maybe_unused]];

public:
    HirVerifier(const HirModule &module, diagnostics::DiagnosticEngine &diags);

    bool verify();
};

} // namespace zith::hir
