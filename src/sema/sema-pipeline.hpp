#pragma once

#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "import/symbol-table.hpp"
#include "memory/dyn-array.hpp"
#include "sema/sema-context.hpp"
#include "sema/sema-result.hpp"
#include "types/type-intern.hpp"
#include "types/type-lower.hpp"
#include "types/unify.hpp"
#include "zir/hir/hir-module.hpp"

namespace zith::sema {

class SemaPipeline {
    SemaContext ctx_;
    types::Unifier unifier_;
    memory::Arena &hir_arena_;
    zir::hir::HirModule hir_;
    zir::hir::HirFunction *current_fn_                  = nullptr;
    types::TypeId current_fn_ret_type_                  = types::kVoidType;
    const memory::DynArray<import::SymId> *resolved_    = nullptr;
    memory::DynArray<types::TypeId> hir_types_;

    zir::hir::HirExprId addHirExpr(zir::hir::HirExpr expr, types::TypeId type);
    types::TypeId exprType(zir::hir::HirExprId id) const;

    zir::hir::HirExprId visitExpr(ast::ExprId id);
    zir::hir::HirExprId visitLiteral(const ast::LitValue &n);
    zir::hir::HirExprId visitIdent(const ast::IdentNode &n, ast::ExprId id);
    zir::hir::HirExprId visitBinary(const ast::BinaryNode &n);
    zir::hir::HirExprId visitUnary(const ast::UnaryNode &n);
    zir::hir::HirExprId visitCall(const ast::CallNode &n);
    zir::hir::HirExprId visitBlock(const ast::BlockNode &n);
    zir::hir::HirExprId visitIf(const ast::IfNode &n);
    zir::hir::HirExprId visitWhile(const ast::WhileNode &n);
    void visitStmt(ast::StmtId id);
    void visitDecl(const ast::FnDeclNode &n);
    types::TypeId inferExprType(ast::ExprId id);

public:
    SemaPipeline(import::SymbolTable &syms, types::TypeIntern &types,
                 diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                 memory::Arena &hir_arena,
                 const memory::DynArray<import::SymId> *resolved = nullptr);

    import::SymbolTable &syms() noexcept {
        return ctx_.syms();
    }
    bool run(const ast::ProgramNode &program);
    zir::hir::HirModule takeHir() {
        return std::move(hir_);
    }
    const zir::hir::HirModule &hir() const {
        return hir_;
    }
};

} // namespace zith::sema
