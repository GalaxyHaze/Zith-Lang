#include "unify.hpp"

namespace zith::middleend::types {

    Unifier::Unifier(TypeIntern &intern, diagnostics::engine::DiagnosticEngine &diags) :
        intern_(intern), diags_(diags) {}

    TypeId Unifier::freshVar() {
        return kErrorType;
    }
    bool Unifier::unify(TypeId a, TypeId b) {
        (void)a;
        (void)b;
        return false;
    }
    TypeId Unifier::substitute(TypeId t) const {
        return t;
    }
    bool Unifier::occurs(TypeId var, TypeId t) const {
        (void)var;
        (void)t;
        return false;
    }

} // namespace zith::middleend::types
