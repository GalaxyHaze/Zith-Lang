#pragma once

#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"
#include "memory/dyn-array.hpp"
#include "memory/flat-map.hpp"
#include "sema/sema-context.hpp"
#include "sema/typed-ast.hpp"
#include "symbols/import-manager.hpp"
#include "symbols/symbol-table.hpp"
#include "types/type-intern.hpp"
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
    TypedAst typed_ast_;
    symbols::ImportManager *import_mgr_ = nullptr;
    memory::DynArray<symbols::SymId> worklist_;
    memory::DynArray<symbols::SymId> fn_syms_;
    ast::AstBuilder *active_builder_ = nullptr;
    memory::DynArray<size_t> allowed_files_;

    memory::DynArray<types::TypeId> var_types_;

    hir::HirExprId addHirExpr(hir::HirExpr expr, types::TypeId type,
                              ast::ExprId source_id = ast::kInvalidExpr);
    types::TypeId current_fn_ret_type_ = types::kErrorType;
    types::TypeId exprType(hir::HirExprId id) const;
    types::TypeId astExprType(ast::ExprId id) const;
    ast::AstBuilder &builder() const;
    void ensureStub(symbols::SymId fn_sym);
    void ensureBodyLowered(symbols::SymId fn_sym);
    bool isSymAccessible(symbols::SymId sym_id) const;
    ast::AstBuilder *builderForSym(symbols::SymId fn_sym);
    const symbols::SymbolTable *symsForSym(symbols::SymId fn_sym);
    size_t hirIndexForSym(symbols::SymId fn_sym) const;

    hir::HirExprId visitExpr(ast::ExprId id);
    hir::HirExprId visitLiteral(ast::ExprId id, const ast::LitValue &n);
    hir::HirExprId visitIdent(const ast::IdentNode &n, ast::ExprId id);
    hir::HirExprId visitBinary(ast::ExprId id, const ast::BinaryNode &n);
    hir::HirExprId visitUnary(ast::ExprId id, const ast::UnaryNode &n);
    hir::HirExprId visitCall(ast::ExprId id, const ast::CallNode &n);
    hir::HirExprId visitBlock(ast::ExprId id, const ast::BlockNode &n);
    hir::HirExprId visitIf(ast::ExprId id, const ast::IfNode &n);
    hir::HirExprId visitWhile(ast::ExprId id, const ast::WhileNode &n);
    void visitStmt(ast::StmtId id);

    // ── block management ──
    size_t currentBlock() const {
        return currentBlock_;
    }
    void setCurrentBlock(size_t idx) {
        currentBlock_ = idx;
    }
    size_t newBlock();
    void setTerminator(hir::HirExprId term);
    void visitMarker(const ast::MarkerNode &n);
    void visitGoto(const ast::GotoNode &n);
    hir::HirExprId emitBranch(ast::ExprId cond, size_t then_block, size_t else_block);
    void emitJump(size_t target);

    size_t currentBlock_ = 0;
    memory::FlatMap<std::string_view, size_t> labelMap_;
    void visitDecl(const ast::FnDeclNode &n);
    types::TypeId inferExprType(ast::ExprId id);

public:
    SemaPipeline(symbols::SymbolTable &syms, types::TypeIntern &types,
                 diagnostics::DiagnosticEngine &diags, ast::AstBuilder &builder,
                 memory::Arena &hir_arena,
                 const memory::DynArray<symbols::SymId> *resolved = nullptr,
                 symbols::ImportManager *import_mgr               = nullptr);

    symbols::SymbolTable &syms() noexcept {
        return ctx_.syms();
    }
    bool run(const ast::ProgramNode &program);
    hir::HirModule takeHir() {
        return std::move(hir_);
    }
    TypedAst takeTypedAst() {
        return std::move(typed_ast_);
    }
    const hir::HirModule &hir() const {
        return hir_;
    }
    const TypedAst &typedAst() const {
        return typed_ast_;
    }
};

} // namespace zith::sema
