#include "hir-verify.hpp"

namespace zith::zir::hir {

    HirVerifier::HirVerifier(const HirModule &module,
                             diagnostics::DiagnosticEngine &diags) :
        module_(module), diags_(diags) {}

    bool HirVerifier::verify() {
        return true;
    }

} // namespace zith::zir::hir
