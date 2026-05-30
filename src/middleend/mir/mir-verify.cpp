#include "mir-verify.hpp"

namespace zith::middleend::mir {

    MirVerifier::MirVerifier(const MirModule& module, diagnostics::engine::DiagnosticEngine& diags)
        : module_(module), diags_(diags) {}

    bool MirVerifier::verify() { return true; }

}
