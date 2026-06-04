#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "frontend/ast/ast-builder.hpp"
#include "middleend/symbols/symbol-table.hpp"

namespace zith::middleend::symbols {

    class Resolver {
        SymbolTable &syms_;
        frontend::ast::AstBuilder &builder_;
        diagnostics::engine::DiagnosticEngine &diags_;

    public:
        Resolver(SymbolTable &syms,
                 frontend::ast::AstBuilder &builder,
                 diagnostics::engine::DiagnosticEngine &diags);

        void resolveProgram(frontend::ast::DeclId program);

    private:
        void resolveDecl(frontend::ast::DeclId id);
        void resolveStmt(frontend::ast::StmtId id);
        void resolveExpr(frontend::ast::ExprId id);
    };

} // namespace zith::middleend::symbols
