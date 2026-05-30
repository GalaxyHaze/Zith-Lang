#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "frontend/ast/ast-builder.hpp"
#include "middleend/sema/sema-context.hpp"
#include "middleend/sema/sema-result.hpp"
#include "middleend/symbols/symbol-table.hpp"
#include "middleend/types/type-intern.hpp"
#include "middleend/types/unify.hpp"

namespace zith::middleend::sema {

    class SemaPipeline {
        SemaContext ctx_;
        types::Unifier unifier_;
        hir::HirModule hir_;

    public:
        SemaPipeline(symbols::SymbolTable& syms, types::TypeIntern& types,
                     diagnostics::engine::DiagnosticEngine& diags,
                     frontend::ast::AstBuilder& builder);

        SemaResult run(frontend::ast::DeclId program);
    };

}
