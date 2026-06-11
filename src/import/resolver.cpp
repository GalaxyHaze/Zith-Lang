#include "resolver.hpp"

namespace zith::import {

Resolver::Resolver(SymbolTable &syms, ast::AstBuilder &builder,
                   diagnostics::DiagnosticEngine &diags)
    : syms_(syms), builder_(builder), diags_(diags) {}

void Resolver::resolveProgram(ast::ProgramNode &) {}
void Resolver::resolveDecl(ast::DeclId) {}
void Resolver::resolveStmt(ast::StmtId) {}
void Resolver::resolveExpr(ast::ExprId) {}

} // namespace zith::import
