#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/mir/mir-module.hpp"

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
