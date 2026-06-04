#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "middleend/hir/hir-module.hpp"
#include "middleend/mir/mir-module.hpp"

namespace zith::middleend::mir {

    class MirLowering {
        const hir::HirModule &hir_;
        diagnostics::engine::DiagnosticEngine &diags_;
        MirModule mir_;

    public:
        MirLowering(const hir::HirModule &hir,
                    infra::alloc::Arena &arena,
                    diagnostics::engine::DiagnosticEngine &diags);

        MirModule lower();
    };

} // namespace zith::middleend::mir
