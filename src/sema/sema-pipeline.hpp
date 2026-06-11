#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-builder.hpp"
#include "sema/sema-context.hpp"
#include "sema/sema-result.hpp"
#include "import/symbol-table.hpp"
#include "types/type-intern.hpp"
#include "types/unify.hpp"

namespace zith::sema {

    class SemaPipeline {
        SemaContext ctx_;
        types::Unifier unifier_;
        zir::hir::HirModule hir_;

    public:
        SemaPipeline(import::SymbolTable &syms,
                     types::TypeIntern &types,
                     diagnostics::DiagnosticEngine &diags,
                     ast::AstBuilder &builder,
                     memory::Arena &hir_arena);

        SemaResult run(ast::DeclId program);
    };

} // namespace zith::sema
