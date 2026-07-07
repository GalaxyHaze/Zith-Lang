#include "hir-verify.hpp"

namespace zith::hir {

HirVerifier::HirVerifier(const HirModule &module, diagnostics::DiagnosticEngine &diags)
    : module_(module), diags_(diags) {}

bool HirVerifier::verify() {
    return true;
}

} // namespace zith::hir
