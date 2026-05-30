#include "mir-lower-from-hir.hpp"

namespace zith::middleend::mir {

    MirLowering::MirLowering(const hir::HirModule& hir, infra::alloc::Arena& arena,
                             diagnostics::engine::DiagnosticEngine& diags)
        : hir_(hir), diags_(diags), mir_(arena) {}

    MirModule MirLowering::lower() {
        return std::move(mir_);
    }

}
