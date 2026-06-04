#include "mir-lower-from-hir.hpp"

namespace zith::zir::mir {

    MirLowering::MirLowering(const hir::HirModule &hir,
                             memory::Arena &arena,
                             diagnostics::DiagnosticEngine &diags) :
        hir_(hir), diags_(diags), mir_(arena) {}

    MirModule MirLowering::lower() {
        return std::move(mir_);
    }

} // namespace zith::zir::mir
