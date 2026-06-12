#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "memory/dyn-array.hpp"
#include "types/type-intern.hpp"

namespace zith::types {

class Unifier {
    TypeIntern &intern_;
    diagnostics::DiagnosticEngine &diags_;
    memory::DynArray<TypeId> subst_;

public:
    Unifier(TypeIntern &intern, diagnostics::DiagnosticEngine &diags, memory::Arena &arena);

    TypeId freshVar();

    bool unify(TypeId a, TypeId b, memory::Span span = {});
    TypeId substitute(TypeId t) const;
    bool occurs(TypeId var, TypeId t) const;

    bool isAssignable(TypeId dst, TypeId src) const;
    bool isCoercible(TypeId dst, TypeId src) const;
};

} // namespace zith::types
