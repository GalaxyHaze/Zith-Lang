#include "sema-pipeline.hpp"

namespace zith::sema {

    SemaPipeline::SemaPipeline(import::SymbolTable &syms,
                               types::TypeIntern &types,
                               diagnostics::DiagnosticEngine &diags,
                               ast::AstBuilder &builder) :
        ctx_(syms, types, diags, builder), unifier_(types, diags),
        hir_(memory::SessionArena) {}

    SemaResult SemaPipeline::run(ast::DeclId program) {
        (void)program;
        return SemaResult{std::move(hir_), std::move(ctx_.diags()), std::move(ctx_.syms())};
    }

} // namespace zith::sema
