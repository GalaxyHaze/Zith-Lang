#pragma once

#include "zir/hir/hir-types.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace zith::zir::mir {

using ValueId = uint32_t;
using BlockId = uint32_t;

inline constexpr ValueId kInvalidValue = ~ValueId{0};
inline constexpr BlockId kInvalidBlock = ~BlockId{0};

enum class MirOpcode : uint8_t {
    Nop,
    Mov,
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    And,
    Or,
    Xor,
    Shl,
    Shr,
    CmpEq,
    CmpNe,
    CmpLt,
    CmpLe,
    CmpGt,
    CmpGe,
    Load,
    Store,
    Alloca,
    Call,
    Ret,
    Jmp,
    Br,
    Phi
};

struct MirOperand {
    ValueId value;
    bool is_immediate = false;
};

struct MirInst {
    MirOpcode opcode;
    hir::HirTypeId type;
    memory::DynArray<MirOperand> operands;

    explicit MirInst(memory::Arena &arena) : operands(arena) {}
};

struct MirBasicBlock {
    BlockId id;
    memory::DynArray<MirInst> insts;
    MirInst terminator;

    explicit MirBasicBlock(memory::Arena &arena) : insts(arena), terminator(arena) {}
};

} // namespace zith::zir::mir
