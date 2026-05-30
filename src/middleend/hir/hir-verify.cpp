#include "hir-verify.hpp"

namespace zith::middleend::hir {

    HirVerifier::HirVerifier(const HirModule &module,
                             diagnostics::engine::DiagnosticEngine &diags) :
        module_(module), diags_(diags) {}

    bool HirVerifier::verify() {
        return true;
    }

} // namespace zith::middleend::hir
