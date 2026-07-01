#pragma once

#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"
#include "memory/dyn-array.hpp"
#include "sema/sema-context.hpp"
#include "sema/sema-result.hpp"
#include "symbols/symbol-table.hpp"
#include "types/type-intern.hpp"
#include "types/type-lower.hpp"
#include "types/unify.hpp"

namespace zith::sema {

class SemaPipeline {
    SemaContext ctx_;
    types::Unifier unifier_;
    memory::Arena &hir_arena_;
    hir::HirModule hir_;
    hir::HirFunction *current_fn_                     = nullptr;
    const memory::DynArray<symbols::SymId> *resolved_ = nullptr;
    memory::DynArray<types::TypeId> hir_types_;

    hir::HirExprId addHirExpr(hir::HirExpr expr, types::TypeId type);
    types::TypeId current_fn_ret_type_ = types::kErrorType;
    types::TypeId exprType(hir::HirExprId id) const;

    hir::HirExprId visitExpr(ast::ExprId id);
    hir::HirExprId visitLiteral(const ast::LitValue &n);
    hir::HirExprId visitIdent(const ast::IdentNode &n, ast::ExprId id);
    hir::HirExprId visitBinary(const ast::BinaryNode &n);
    hir::HirExprId visitUnary(const ast::UnaryNode &n);
    hir::HirExprId visitCall(const ast::CallNode &n);
    hir::HirExprId visitBlock(const ast::BlockNode &n);
    hir::HirExprId visitIf(const ast::IfNode &n);
    hir::HirExprId visitWhile(const ast::WhileNode &n);
    void visitStmt(ast::StmtId id);
    void visitDecl(const ast::FnDeclNode &n);
    types::TypeId inferExprType(ast::ExprId id);

public:
    SemaPipeline(symbols::SymbolTable &syms, types::TypeIntern &types,
                 diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                 memory::Arena &hir_arena,
                 const memory::DynArray<symbols::SymId> *resolved = nullptr);

    symbols::SymbolTable &syms() noexcept {
        return ctx_.syms();
    }
    bool run(const ast::ProgramNode &program);
    hir::HirModule takeHir() {
        return std::move(hir_);
    }
    const hir::HirModule &hir() const {
        return hir_;
    }
};

} // namespace zith::sema
