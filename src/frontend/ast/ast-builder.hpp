#pragma once

#include "frontend/ast/ast-nodes.hpp"
#include "infra/alloc/arena.hpp"
#include "infra/collections/dyn-array.hpp"
#include <string_view>

namespace zith::frontend::ast {

    class AstBuilder {
        infra::collections::DynArray<ExprNode> exprs_;
        infra::collections::DynArray<StmtNode> stmts_;
        infra::collections::DynArray<DeclNode> decls_;

    public:
        explicit AstBuilder(infra::alloc::Arena& arena);

        ExprId addExpr(ExprNode node);
        StmtId addStmt(StmtNode node);
        DeclId addDecl(DeclNode node);

        ExprNode& getExpr(ExprId id);
        StmtNode& getStmt(StmtId id);
        DeclNode& getDecl(DeclId id);

        const ExprNode& getExpr(ExprId id) const;
        const StmtNode& getStmt(StmtId id) const;
        const DeclNode& getDecl(DeclId id) const;

        LitValue makeLit(LitKind kind, std::string_view raw);
        ExprId litExpr(LitKind kind, std::string_view raw);
        ExprId ident(std::string_view name);
        ExprId binary(ExprId lhs, BinaryOp op, ExprId rhs);
        ExprId unary(UnaryOp op, ExprId operand);
        ExprId call(ExprId callee, std::vector<ExprId> args);
        ExprId block(std::vector<StmtId> stmts, ExprId trailing = kInvalidExpr);
        ExprId ifExpr(ExprId cond, ExprId then_branch, ExprId else_branch = kInvalidExpr);

        StmtId letStmt(std::string_view name, bool mut, ExprId init = kInvalidExpr);
        StmtId assign(ExprId target, ExprId value);
        StmtId retStmt(ExprId value = kInvalidExpr);

        DeclId fnDecl(std::string_view name, std::vector<std::string_view> params, ExprId body = kInvalidExpr);
        DeclId structDecl(std::string_view name, std::vector<std::pair<std::string_view, uint32_t>> fields);
        DeclId importDecl(std::vector<std::string_view> path, std::string_view alias = {});

        infra::alloc::Arena& arena();
    };

}
