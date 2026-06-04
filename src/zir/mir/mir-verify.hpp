#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/mir/mir-module.hpp"

namespace zith::zir::mir {

    class MirVerifier {
        const MirModule &module_;
        diagnostics::DiagnosticEngine &diags_;

    public:
        MirVerifier(const MirModule &module, diagnostics::DiagnosticEngine &diags);

        bool verify();
    };

} // namespace zith::zir::mir
