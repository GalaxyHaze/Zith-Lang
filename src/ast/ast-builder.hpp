#pragma once

#include "ast/ast-nodes.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"

namespace zith::ast {

class AstBuilder {
    memory::Arena &arena_;
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
    ExprId call(ExprId callee, memory::DynArray<ExprId> args);
    ExprId field(ExprId object, std::string_view field_name);
    ExprId index(ExprId object, ExprId index);
    ExprId block(memory::DynArray<StmtId> stmts, ExprId trailing = kInvalidExpr);
    ExprId ifExpr(ExprId cond, ExprId then_branch, ExprId else_branch = kInvalidExpr);
    ExprId whileExpr(ExprId cond, ExprId body);

    StmtId letStmt(std::string_view name, bool mut, ExprId init = kInvalidExpr);
    StmtId assign(ExprId target, ExprId value);
    StmtId retStmt(ExprId value = kInvalidExpr);

    DeclId fnDecl(std::string_view name, memory::DynArray<std::string_view> params,
                  ExprId body = kInvalidExpr);
    DeclId structDecl(std::string_view name,
                      memory::DynArray<StructField> fields);
    DeclId enumDecl(std::string_view name,
                    memory::DynArray<EnumVariant> variants);
    DeclId unionDecl(std::string_view name,
                     memory::DynArray<UnionVariant> variants);
    DeclId componentDecl(std::string_view name);
    DeclId traitDecl(std::string_view name,
                     memory::DynArray<TraitMethod> methods);
    DeclId interfaceDecl(std::string_view name,
                         memory::DynArray<TraitMethod> methods);
    DeclId importDecl(memory::DynArray<std::string_view> path, std::string_view alias = {},
                      bool is_from = false, bool is_export = false, int32_t import_depth = 1);

    ExprId unbody(memory::Span body_span, uint32_t token_start, uint32_t token_end);

    memory::Arena &arena();
};

} // namespace zith::ast
