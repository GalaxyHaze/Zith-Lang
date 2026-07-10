#include "ast-builder.hpp"

namespace zith::ast {

AstBuilder::AstBuilder(memory::Arena &arena, memory::StringInterner &interner)
    : arena_(arena), interner_(interner), exprs_(arena), stmts_(arena), decls_(arena), type_exprs_(arena) {}

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

LitValue AstBuilder::makeLit(LitKind kind, std::string_view raw, memory::Span span) {
    return LitValue{kind, raw, span};
}

ExprId AstBuilder::litExpr(LitKind kind, std::string_view raw, memory::Span span) {
    return addExpr(makeLit(kind, raw, span));
}

ExprId AstBuilder::ident(std::string_view name, memory::Span span) {
    return addExpr(IdentNode{name, span});
}

ExprId AstBuilder::binary(ExprId lhs, BinaryOp op, ExprId rhs, memory::Span span) {
    return addExpr(BinaryNode{lhs, rhs, op, span});
}

ExprId AstBuilder::unary(UnaryOp op, ExprId operand, memory::Span span) {
    return addExpr(UnaryNode{operand, op, span});
}

ExprId AstBuilder::call(ExprId callee, memory::DynArray<ExprId> args, memory::Span span) {
    return addExpr(CallNode{callee, std::move(args), span});
}

ExprId AstBuilder::field(ExprId object, std::string_view field_name, memory::Span span) {
    return addExpr(FieldNode{object, field_name, span});
}

ExprId AstBuilder::index(ExprId object, ExprId index, memory::Span span) {
    return addExpr(IndexNode{object, index, span});
}

ExprId AstBuilder::range(ExprId lhs, ExprId rhs, memory::Span span) {
    return addExpr(RangeNode{lhs, rhs, span});
}

ExprId AstBuilder::block(memory::DynArray<StmtId> stmts, ExprId trailing, memory::Span span) {
    return addExpr(BlockNode{std::move(stmts), trailing, span});
}

ExprId AstBuilder::ifExpr(ExprId cond, ExprId then_branch, ExprId else_branch, memory::Span span) {
    return addExpr(IfNode{cond, then_branch, else_branch, span});
}

ExprId AstBuilder::whileExpr(ExprId cond, ExprId body, memory::Span span) {
    return addExpr(WhileNode{cond, body, span});
}

StmtId AstBuilder::letStmt(memory::DynArray<std::string_view> names, bool mut,
                           TypeExprId type_annot, ExprId init, memory::Span span) {
    return addStmt(LetNode{mut, type_annot, std::move(names), init, span});
}

StmtId AstBuilder::letStmt(std::string_view name, bool mut, ExprId init, memory::Span span) {
    memory::DynArray<std::string_view> names{arena_};
    names.push(name);
    return addStmt(LetNode{mut, kInvalidTypeExpr, std::move(names), init, span});
}

StmtId AstBuilder::assign(ExprId target, ExprId value, memory::Span span) {
    return addStmt(AssignNode{target, value, span});
}

StmtId AstBuilder::retStmt(ExprId value, memory::Span span) {
    return addStmt(RetNode{value, span});
}

DeclId AstBuilder::fnDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                          memory::DynArray<FnParam> params, TypeExprId return_type, ExprId body,
                          memory::Span span) {
    return addDecl(
        FnDeclNode{name, std::move(generic_params), std::move(params), return_type, body, span});
}

DeclId AstBuilder::fnDecl(std::string_view name, memory::DynArray<std::string_view> param_names,
                          ExprId body, memory::Span span) {
    memory::DynArray<FnParam> params{arena_};
    for (auto &pn : param_names)
        params.push({pn, kInvalidTypeExpr});
    return addDecl(FnDeclNode{name, memory::DynArray<GenericParam>{arena_}, std::move(params),
                              kInvalidTypeExpr, body, span});
}

DeclId AstBuilder::structDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                              memory::DynArray<StructField> fields,
                              memory::DynArray<DeclId> methods,
                              TypeExprId extends_parent, memory::Span span) {
    memory::DynArray<TypeExprId> empty_traits{arena_};
    return addDecl(StructDeclNode{name, std::move(generic_params), std::move(fields),
                                  std::move(methods), extends_parent, std::move(empty_traits), span});
}

DeclId AstBuilder::enumDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                            memory::DynArray<EnumVariant> variants, memory::Span span) {
    return addDecl(EnumDeclNode{name, std::move(generic_params), std::move(variants), span});
}

DeclId AstBuilder::unionDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                             memory::DynArray<UnionVariant> variants, memory::Span span) {
    return addDecl(UnionDeclNode{name, std::move(generic_params), std::move(variants), span});
}

DeclId AstBuilder::componentDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                                 memory::DynArray<StructField> fields, memory::Span span) {
    return addDecl(ComponentDeclNode{name, std::move(generic_params), std::move(fields),
                                    memory::DynArray<ast::DeclId>(arena_), span});
}

DeclId AstBuilder::traitDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                             memory::DynArray<TraitMethod> methods, memory::Span span) {
    return addDecl(TraitDeclNode{name, std::move(generic_params), std::move(methods), span});
}

DeclId AstBuilder::interfaceDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                                 memory::DynArray<TraitMethod> methods, memory::Span span) {
    return addDecl(InterfaceDeclNode{name, std::move(generic_params), std::move(methods), span});
}

DeclId AstBuilder::importDecl(memory::DynArray<std::string_view> path,
                              memory::DynArray<ImportSymbol> symbols,
                              std::string_view alias,
                              bool is_from, bool is_export, bool is_asset,
                              int32_t import_depth,
                              memory::Span span) {
    return addDecl(ImportNode{std::move(path), std::move(symbols), alias,
                              is_from, is_export, is_asset, import_depth, span});
}

DeclId AstBuilder::typeAliasDecl(std::string_view name, memory::DynArray<GenericParam> generic_params,
                                  TypeExprId target_type, memory::Span span) {
    return addDecl(TypeAliasDeclNode{name, std::move(generic_params), target_type, span});
}

DeclId AstBuilder::globalDecl(std::string_view name, bool is_const,
                              TypeExprId type_annot, ExprId init, memory::Span span) {
    return addDecl(GlobalDeclNode{name, is_const, type_annot, init, span});
}

ExprId AstBuilder::unbody(memory::Span body_span, uint32_t token_start, uint32_t token_end) {
    return addExpr(UnbodyNode{body_span, token_start, token_end});
}

ExprId AstBuilder::intrinsic(IntrinsicKind kind, memory::DynArray<ExprId> args, memory::Span span) {
    return addExpr(IntrinsicNode{kind, std::move(args), span});
}

ExprId AstBuilder::macroCall(std::string_view name, memory::DynArray<ExprId> args, memory::Span span) {
    return addExpr(MacroCallNode{name, std::move(args), span});
}

// ── Type expression helpers ─────────────────────────────────────────

TypeExprId AstBuilder::addTypeExpr(TypeExprNode node) {
    TypeExprId id = static_cast<TypeExprId>(type_exprs_.size());
    type_exprs_.push(std::move(node));
    return id;
}

TypeExprNode &AstBuilder::getTypeExpr(TypeExprId id) {
    return type_exprs_[id];
}

const TypeExprNode &AstBuilder::getTypeExpr(TypeExprId id) const {
    return type_exprs_[id];
}

TypeExprId AstBuilder::builtinExpr(BuiltinType kind) {
    return addTypeExpr(TypeBuiltin{kind});
}

TypeExprId AstBuilder::pathExpr(memory::DynArray<std::string_view> segments, memory::Span span) {
    TypePath p{std::move(segments)};
    p.span = span;
    return addTypeExpr(std::move(p));
}

TypeExprId AstBuilder::inferExpr() {
    return addTypeExpr(TypeInfer{});
}

memory::Span AstBuilder::exprSpan(ExprId id) const {
    return std::visit([](auto &node) { return node.span; }, exprs_[id]);
}

memory::Span AstBuilder::stmtSpan(StmtId id) const {
    auto &stmt = stmts_[id];
    switch (stmt.index()) {
    case 0:
        return std::get<ast::LetNode>(stmt).span;
    case 1:
        return std::get<ast::AssignNode>(stmt).span;
    case 2:
        return std::get<ast::RetNode>(stmt).span;
    case 3:
        return exprSpan(std::get<ExprId>(stmt));
    }
    return {};
}

memory::Span AstBuilder::declSpan(DeclId id) const {
    return std::visit([](auto &node) { return node.span; }, decls_[id]);
}

memory::Arena &AstBuilder::arena() {
    return arena_;
}
memory::StringInterner &AstBuilder::interner() {
    return interner_;
}

} // namespace zith::ast
