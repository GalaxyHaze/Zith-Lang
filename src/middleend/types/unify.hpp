#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "middleend/types/type-intern.hpp"

#include <vector>

namespace zith::middleend::types {

    class Unifier {
        TypeIntern &intern_;
        diagnostics::engine::DiagnosticEngine &diags_;
        std::vector<TypeId> subst_;

    public:
        Unifier(TypeIntern &intern, diagnostics::engine::DiagnosticEngine &diags);

        TypeId freshVar();
        bool unify(TypeId a, TypeId b);
        TypeId substitute(TypeId t) const;

        bool occurs(TypeId var, TypeId t) const;
    };

} // namespace zith::middleend::types
