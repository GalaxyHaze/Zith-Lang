#include "resolver.hpp"

namespace zith::middleend::symbols {

    Resolver::Resolver(SymbolTable &syms,
                       frontend::ast::AstBuilder &builder,
                       diagnostics::engine::DiagnosticEngine &diags) :
        syms_(syms), builder_(builder), diags_(diags) {}

    void Resolver::resolveProgram(frontend::ast::DeclId) {}
    void Resolver::resolveDecl(frontend::ast::DeclId) {}
    void Resolver::resolveStmt(frontend::ast::StmtId) {}
    void Resolver::resolveExpr(frontend::ast::ExprId) {}

} // namespace zith::middleend::symbols
