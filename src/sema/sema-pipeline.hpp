#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-builder.hpp"
#include "sema/sema-context.hpp"
#include "sema/sema-result.hpp"
#include "import/symbol-table.hpp"
#include "types/type-intern.hpp"
#include "types/unify.hpp"

namespace zith::middleend::sema {

    class SemaPipeline {
        SemaContext ctx_;
        types::Unifier unifier_;
        hir::HirModule hir_;

    public:
        SemaPipeline(symbols::SymbolTable &syms,
                     types::TypeIntern &types,
                     diagnostics::engine::DiagnosticEngine &diags,
                     frontend::ast::AstBuilder &builder);

        SemaResult run(frontend::ast::DeclId program);
    };

} // namespace zith::middleend::sema
