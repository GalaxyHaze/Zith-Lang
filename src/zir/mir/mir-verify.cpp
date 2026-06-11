#include "mir-verify.hpp"

namespace zith::zir::mir {

MirVerifier::MirVerifier(const MirModule &module, diagnostics::DiagnosticEngine &diags)
    : module_(module), diags_(diags) {}

bool MirVerifier::verify() {
    return true;
}

} // namespace zith::zir::mir
