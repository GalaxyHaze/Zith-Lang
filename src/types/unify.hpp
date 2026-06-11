#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "types/type-intern.hpp"

#include <vector>

namespace zith::types {

class Unifier {
    TypeIntern &intern_;
    diagnostics::DiagnosticEngine &diags_;
    memory::DynArray<TypeId> subst_;

public:
    Unifier(TypeIntern &intern, diagnostics::DiagnosticEngine &diags, memory::Arena &arena);

    TypeId freshVar();
    bool unify(TypeId a, TypeId b);
    TypeId substitute(TypeId t) const;

    bool occurs(TypeId var, TypeId t) const;
};

} // namespace zith::types
