#pragma once

#include <cstdint>

namespace zith::frontend::ast {

    enum class NodeKind : uint8_t {
        Literal,
        Identifier,
        Binary,
        Unary,
        Call,
        Block,
        If,
        Let,
        Assign,
        Return,
        FnDecl,
        StructDecl,
        Import,
        Program
    };

    using NodeId = uint32_t;
    using ExprId = uint32_t;
    using StmtId = uint32_t;
    using DeclId = uint32_t;

    inline constexpr NodeId kInvalidNode = ~NodeId{0};
    inline constexpr ExprId kInvalidExpr = ~ExprId{0};
    inline constexpr StmtId kInvalidStmt = ~StmtId{0};
    inline constexpr DeclId kInvalidDecl = ~DeclId{0};

}
