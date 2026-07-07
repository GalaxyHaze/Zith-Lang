#pragma once

#include "ast/ast-ids.hpp"
#include "memory/dyn-array.hpp"
#include "memory/span.hpp"

#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

namespace zith::ast {

enum class BinaryOp : uint8_t {
    Add,
    Sub,
    Mul,
    Div,
    Rest,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
    Xor,
    Shl,
    Shr
};

enum class UnaryOp : uint8_t { Neg, Not, Ref, Deref };

enum class LitKind : uint8_t { Int, Float, Bool, String, Char, Nil };

struct LitValue {
    LitKind kind;
    std::string_view raw;
    memory::Span span{};
};

struct BinaryNode {
    ExprId lhs;
    ExprId rhs;
    BinaryOp op;
    memory::Span span{};
};

struct UnaryNode {
    ExprId operand;
    UnaryOp op;
    memory::Span span{};
};

struct CallNode {
    ExprId callee;
    memory::DynArray<ExprId> args;
    memory::Span span{};
};

struct IdentNode {
    std::string_view name;
    memory::Span span{};
};

struct FieldNode {
    ExprId object;
    std::string_view field;
    memory::Span span{};
};

struct IndexNode {
    ExprId object;
    ExprId index;
    memory::Span span{};
};

struct RangeNode {
    ExprId lhs;
    ExprId rhs;
    memory::Span span{};
};

struct BlockNode {
    memory::DynArray<StmtId> stmts;
    ExprId trailing = kInvalidExpr;
    memory::Span span{};
};

struct IfNode {
    ExprId cond;
    ExprId then_branch;
    ExprId else_branch = kInvalidExpr;
    memory::Span span{};
};

struct WhileNode {
    ExprId cond;
    ExprId body;
    memory::Span span{};
};

struct FnParam {
    std::string_view name;
    TypeExprId type = kInvalidTypeExpr;
};

struct GenericParam {
    std::string_view name;
    memory::DynArray<TypeExprId> bounds;
};

struct LetNode {
    bool mut              = false;
    TypeExprId type_annot = kInvalidTypeExpr;
    memory::DynArray<std::string_view> names;
    ExprId init = kInvalidExpr;
    memory::Span span{};
};

struct AssignNode {
    ExprId target;
    ExprId value;
    memory::Span span{};
};

struct RetNode {
    ExprId value = kInvalidExpr;
    memory::Span span{};
};

struct FnDeclNode {
    std::string_view name;
    memory::DynArray<GenericParam> generic_params;
    memory::DynArray<FnParam> params;
    TypeExprId return_type = kInvalidTypeExpr;
    ExprId body            = kInvalidExpr;
    memory::Span span{};
};

struct StructField {
    std::string_view name;
    TypeExprId type_expr = kInvalidTypeExpr;
};

struct StructDeclNode {
    std::string_view name;
    memory::DynArray<StructField> fields;
    memory::Span span{};
};

struct EnumVariant {
    std::string_view name;
    memory::DynArray<StructField> fields;
    int64_t discriminant = -1;
};

struct EnumDeclNode {
    std::string_view name;
    memory::DynArray<EnumVariant> variants;
    memory::Span span{};
};

struct UnionVariant {
    std::string_view name;
    TypeExprId type_expr = kInvalidTypeExpr;
};

struct UnionDeclNode {
    std::string_view name;
    memory::DynArray<UnionVariant> variants;
    memory::Span span{};
};

struct ComponentDeclNode {
    std::string_view name;
    memory::Span span{};
};

struct TraitMethod {
    std::string_view name;
    memory::DynArray<std::string_view> params;
};

struct TraitDeclNode {
    std::string_view name;
    memory::DynArray<TraitMethod> methods;
    memory::Span span{};
};

struct InterfaceDeclNode {
    std::string_view name;
    memory::DynArray<TraitMethod> methods;
    memory::Span span{};
};

struct ImportNode {
    memory::DynArray<std::string_view> path;
    std::string_view alias;
    bool is_from         = false;
    bool is_export       = false;
    int32_t import_depth = 1;
    memory::Span span{};
};

struct UnbodyNode {
    memory::Span body_span;
    uint32_t token_start;
    uint32_t token_end;
    memory::Span span{};
};

struct ProgramNode {
    memory::DynArray<DeclId> decls;

    explicit ProgramNode(memory::Arena &arena) : decls(arena) {}
};

enum class IntrinsicKind : uint8_t {
    Fields,
    SizeOf,
    AlignOf,
    HasTrait,
    Struct,
    Component,
    Union,
    Enum,
    Nullable,
    Primitive,
    Allocate,
    Pack,
    ToStruct,
    ToPack,
    AppendField,
    RemoveField,
    AppendMethod,
    File,
    Line,
    FnName,
    Location,
    Ok,
    Err,
    OffsetOf,
};

struct IntrinsicNode {
    IntrinsicKind kind;
    memory::DynArray<ExprId> args;
    memory::Span span{};
};

struct MacroCallNode {
    std::string_view name;
    memory::DynArray<ExprId> args;
    memory::Span span{};
};

using ExprNode = std::variant<LitValue, IdentNode, BinaryNode, UnaryNode, CallNode, BlockNode,
                              IfNode, WhileNode, FieldNode, IndexNode, RangeNode, UnbodyNode,
                              IntrinsicNode, MacroCallNode>;

using StmtNode = std::variant<LetNode, AssignNode, RetNode, ExprId>;

using DeclNode = std::variant<FnDeclNode, StructDeclNode, EnumDeclNode, UnionDeclNode,
                              ComponentDeclNode, TraitDeclNode, InterfaceDeclNode, ImportNode>;

} // namespace zith::ast
