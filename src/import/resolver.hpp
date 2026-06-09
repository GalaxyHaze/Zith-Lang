#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-builder.hpp"
#include "import/symbol-table.hpp"

namespace zith::import {

    class Resolver {
        SymbolTable &syms_;
        ast::AstBuilder &builder_;
        diagnostics::DiagnosticEngine &diags_;

    public:
        Resolver(SymbolTable &syms,
                 ast::AstBuilder &builder,
                 diagnostics::DiagnosticEngine &diags);

        void resolveProgram(ast::ProgramNode &program);

    private:
        void resolveDecl(ast::DeclId id);
        void resolveStmt(ast::StmtId id);
        void resolveExpr(ast::ExprId id);
    };

} // namespace zith::import
