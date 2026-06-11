#include "unify.hpp"

namespace zith::types {

    Unifier::Unifier(TypeIntern &intern, diagnostics::DiagnosticEngine &diags, memory::Arena &arena) :
        intern_(intern), diags_(diags), subst_(arena) {}

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

} // namespace zith::types
