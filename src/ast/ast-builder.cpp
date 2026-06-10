#include "ast-builder.hpp"

namespace zith::ast {

    AstBuilder::AstBuilder(memory::Arena &arena) :
        exprs_(arena), stmts_(arena), decls_(arena) {}

    ExprId AstBuilder::addExpr(ExprNode node) {
        ExprId id = static_cast<ExprId>(exprs_.size());
        exprs_.push(std::move(node));
        return id;
    }

    StmtId AstBuilder::addStmt(StmtNode node) {
        StmtId id = static_cast<StmtId>(stmts_.size());
        stmts_.push(std::move(node));
        return id;
    }

    DeclId AstBuilder::addDecl(DeclNode node) {
        DeclId id = static_cast<DeclId>(decls_.size());
        decls_.push(std::move(node));
        return id;
    }

    ExprNode &AstBuilder::getExpr(ExprId id) {
        return exprs_[id];
    }
    StmtNode &AstBuilder::getStmt(StmtId id) {
        return stmts_[id];
    }
    DeclNode &AstBuilder::getDecl(DeclId id) {
        return decls_[id];
    }
    const ExprNode &AstBuilder::getExpr(ExprId id) const {
        return exprs_[id];
    }
    const StmtNode &AstBuilder::getStmt(StmtId id) const {
        return stmts_[id];
    }
    const DeclNode &AstBuilder::getDecl(DeclId id) const {
        return decls_[id];
    }

    LitValue AstBuilder::makeLit(LitKind kind, std::string_view raw) {
        return LitValue{kind, raw};
    }

    ExprId AstBuilder::litExpr(LitKind kind, std::string_view raw) {
        return addExpr(makeLit(kind, raw));
    }

    ExprId AstBuilder::ident(std::string_view name) {
        return addExpr(IdentNode{name});
    }

    ExprId AstBuilder::binary(ExprId lhs, BinaryOp op, ExprId rhs) {
        return addExpr(BinaryNode{lhs, rhs, op});
    }

    ExprId AstBuilder::unary(UnaryOp op, ExprId operand) {
        return addExpr(UnaryNode{operand, op});
    }

    ExprId AstBuilder::call(ExprId callee, memory::DynArray<ExprId> args) {
        return addExpr(CallNode{callee, std::move(args)});
    }

    ExprId AstBuilder::block(memory::DynArray<StmtId> stmts, ExprId trailing) {
        return addExpr(BlockNode{std::move(stmts), trailing});
    }

    ExprId AstBuilder::ifExpr(ExprId cond, ExprId then_branch, ExprId else_branch) {
        return addExpr(IfNode{cond, then_branch, else_branch});
    }

    StmtId AstBuilder::letStmt(std::string_view name, bool mut, ExprId init) {
        return addStmt(LetNode{mut, name, init});
    }

    StmtId AstBuilder::assign(ExprId target, ExprId value) {
        return addStmt(AssignNode{target, value});
    }

    StmtId AstBuilder::retStmt(ExprId value) {
        return addStmt(RetNode{value});
    }

    DeclId
    AstBuilder::fnDecl(std::string_view name, memory::DynArray<std::string_view> params, ExprId body) {
        return addDecl(FnDeclNode{name, std::move(params), body});
    }

    DeclId AstBuilder::structDecl(std::string_view name,
                                  memory::DynArray<std::pair<std::string_view, uint32_t>> fields) {
        return addDecl(StructDeclNode{name, std::move(fields)});
    }

    DeclId AstBuilder::importDecl(memory::DynArray<std::string_view> path, std::string_view alias, bool is_from) {
        return addDecl(ImportNode{std::move(path), alias, is_from});
    }

    ExprId AstBuilder::unbody(memory::Span body_span, uint32_t token_start, uint32_t token_end) {
        return addExpr(UnbodyNode{body_span, token_start, token_end});
    }

    memory::Arena &AstBuilder::arena() {
        return memory::SessionArena;
    }

} // namespace zith::ast
