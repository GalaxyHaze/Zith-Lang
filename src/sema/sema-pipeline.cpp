#include "sema-pipeline.hpp"

namespace zith::sema {

SemaPipeline::SemaPipeline(import::SymbolTable &syms, types::TypeIntern &types,
                           diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                           memory::Arena &hir_arena)
    : ctx_(syms, types, diags, builder), unifier_(types, diags, hir_arena), hir_(hir_arena) {}

SemaResult SemaPipeline::run(ast::DeclId program) {
    (void)program;
    return SemaResult{std::move(hir_), std::move(ctx_.diags()), std::move(ctx_.syms())};
}

} // namespace zith::sema
