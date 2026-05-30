#pragma once

#include "frontend/ast/ast-ids.hpp"
#include "frontend/source/span.hpp"
#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

namespace zith::frontend::ast {

    enum class BinaryOp : uint8_t {
        Add, Sub, Mul, Div, Rem,
        Eq, Ne, Lt, Le, Gt, Ge,
        And, Or, Xor,
        Shl, Shr
    };

    enum class UnaryOp : uint8_t {
        Neg, Not, Ref, Deref
    };

    enum class LitKind : uint8_t {
        Int, Float, Bool, String, Char, Nil
    };

    struct LitValue {
        LitKind kind;
        std::string_view raw;
    };

    struct BinaryNode  { ExprId lhs; ExprId rhs; BinaryOp op; };
    struct UnaryNode   { ExprId operand; UnaryOp op; };
    struct CallNode    { ExprId callee; std::vector<ExprId> args; };
    struct IdentNode   { std::string_view name; };
    struct FieldNode   { ExprId object; std::string_view field; };
    struct IndexNode   { ExprId object; ExprId index; };

    struct BlockNode {
        std::vector<StmtId> stmts;
        ExprId trailing = kInvalidExpr;
    };

    struct IfNode {
        ExprId cond;
        ExprId then_branch;
        ExprId else_branch = kInvalidExpr;
    };

    struct LetNode {
        bool mut = false;
        std::string_view name;
        ExprId init = kInvalidExpr;
    };

    struct AssignNode {
        ExprId target;
        ExprId value;
    };

    struct RetNode {
        ExprId value = kInvalidExpr;
    };

    struct FnDeclNode {
        std::string_view name;
        std::vector<std::string_view> params;
        ExprId body = kInvalidExpr;
    };

    struct StructDeclNode {
        std::string_view name;
        std::vector<std::pair<std::string_view, uint32_t>> fields;
    };

    struct ImportNode {
        std::vector<std::string_view> path;
        std::string_view alias;
    };

    struct ProgramNode {
        std::vector<DeclId> decls;
    };

    using ExprNode = std::variant<
        LitValue, IdentNode, BinaryNode, UnaryNode,
        CallNode, BlockNode, IfNode, FieldNode, IndexNode
    >;

    using StmtNode = std::variant<
        LetNode, AssignNode, RetNode, ExprId
    >;

    using DeclNode = std::variant<
        FnDeclNode, StructDeclNode, ImportNode
    >;

}
