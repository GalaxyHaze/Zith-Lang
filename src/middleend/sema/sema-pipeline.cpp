#include "sema-pipeline.hpp"

namespace zith::middleend::sema {

    SemaPipeline::SemaPipeline(symbols::SymbolTable &syms,
                               types::TypeIntern &types,
                               diagnostics::engine::DiagnosticEngine &diags,
                               frontend::ast::AstBuilder &builder) :
        ctx_(syms, types, diags, builder), unifier_(types, diags),
        hir_(infra::alloc::SessionArena) {}

    SemaResult SemaPipeline::run(frontend::ast::DeclId program) {
        (void)program;
        return SemaResult{std::move(hir_), std::move(ctx_.diags()), std::move(ctx_.syms())};
    }

} // namespace zith::middleend::sema
