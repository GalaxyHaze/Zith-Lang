#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/mir/mir-module.hpp"

namespace zith::zir::mir {

class MirLowering {
    const hir::HirModule &hir_;
    diagnostics::DiagnosticEngine &diags_;
    MirModule mir_;

public:
    MirLowering(const hir::HirModule &hir, memory::Arena &arena,
                diagnostics::DiagnosticEngine &diags);

    MirModule lower();
};

} // namespace zith::zir::mir
