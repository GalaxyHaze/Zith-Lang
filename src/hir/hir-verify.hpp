#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"

namespace zith::types {
class TypeIntern;
}

namespace zith::hir {

class HirVerifier {
    const HirModule &module_;
    const types::TypeIntern &types_;
    diagnostics::DiagnosticEngine &diags_;

public:
    HirVerifier(const HirModule &module, const types::TypeIntern &types,
                diagnostics::DiagnosticEngine &diags);

    bool verify();
};

} // namespace zith::hir
