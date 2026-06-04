#pragma once

#include "ast/ast-nodes.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"


namespace zith::ast {

    class AstBuilder {
        memory::DynArray<ExprNode> exprs_;
        memory::DynArray<StmtNode> stmts_;
        memory::DynArray<DeclNode> decls_;

    public:
        explicit AstBuilder(memory::Arena &arena);

        ExprId addExpr(ExprNode node);
        StmtId addStmt(StmtNode node);
        DeclId addDecl(DeclNode node);

        ExprNode &getExpr(ExprId id);
        StmtNode &getStmt(StmtId id);
        DeclNode &getDecl(DeclId id);

        const ExprNode &getExpr(ExprId id) const;
        const StmtNode &getStmt(StmtId id) const;
        const DeclNode &getDecl(DeclId id) const;

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

        DeclId fnDecl(std::string_view name,
                      std::vector<std::string_view> params,
                      ExprId body = kInvalidExpr);
        DeclId structDecl(std::string_view name,
                          std::vector<std::pair<std::string_view, uint32_t>> fields);
        DeclId importDecl(std::vector<std::string_view> path, std::string_view alias = {});

        memory::Arena &arena();
    };

} // namespace zith::ast
