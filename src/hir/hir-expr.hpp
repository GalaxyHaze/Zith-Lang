#pragma once

#include "hir/hir-types.hpp"
#include "memory/dyn-array.hpp"
#include "memory/string-interner.hpp"
#include "symbols/symbol-id.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace zith::hir {

using HirExprId = uint32_t;
using HirDeclId = uint32_t;

inline constexpr HirExprId kInvalidHirExpr = ~HirExprId{0};

enum class HirBinaryOp : uint8_t { Add, Sub, Mul, Div, Rem, Eq, Ne, Lt, Le, Gt, Ge, And, Or, Xor };

enum class HirUnaryOp : uint8_t {
    Neg,
    Not,
    BitNot,
    Ref,
    Deref,
};

struct HirLiteral {
    HirTypeId type;
    union {
        int64_t i;
        double f;
        bool b;
        memory::InternedId str_val;
    };
};

struct HirBinary {
    HirExprId lhs;
    HirExprId rhs;
    HirBinaryOp op;
};
struct HirUnary {
    HirUnaryOp op;
    HirExprId operand;
};
struct HirLet {
    memory::InternedId name;
    HirTypeId type;
    HirExprId init;
};
struct HirVar {
    memory::InternedId name;
    uint32_t version;
};
struct HirCall {
    HirExprId callee;
    memory::DynArray<HirExprId> args;
    symbols::SymId resolved_fn = symbols::kInvalidSym;
};
struct HirRet {
    HirExprId value;
};
struct HirBranch {
    HirExprId cond;
    HirDeclId then_block;
    HirDeclId else_block;
};
struct HirJump {
    HirDeclId target;
};
struct HirPhi {
    memory::DynArray<HirExprId> incoming;
};

using HirExpr = std::variant<HirLiteral, HirBinary, HirUnary, HirLet, HirVar, HirCall, HirRet,
                             HirBranch, HirJump, HirPhi>;

} // namespace zith::hir
