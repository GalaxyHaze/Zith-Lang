#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"

#include <cstdint>
#include <string_view>

namespace zith::ir {

/// Instruction opcodes are intentionally small and explicit. A runtime cannot
/// insert hidden checks: bad operands and unsupported operations surface as
/// explicit trap signs in the contract.
enum class Op : uint8_t {
    LoadConst,
    LoadString,
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    Neg,
    Not,
    BitNot,
    Copy,
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
    Shr,
    SlotLoad,
    SlotStore,
    CallFn,
    CallExtern,
    Ret,
    Branch,
    Jump,
    Trap
};

/// One fixed-size execution IR row. Register indices live in `a/b/c`;
/// table indices and jump offsets live in `imm`.
struct Instr {
    Op op        = Op::Trap;
    uint16_t a   = 0;
    uint16_t b   = 0;
    uint16_t c   = 0;
    uint16_t imm = 0;
};

struct Function {
    std::string_view name;
    uint16_t paramCount     = 0;
    uint16_t registerCount  = 0;
    uint16_t slotCount      = 0;
    uint16_t returnRegister = 0;
    bool returnIsExitCode   = true;
    memory::DynArray<Instr> body;

    explicit Function(memory::Arena &arena) : body(arena) {}
};

struct Module {
    memory::DynArray<Function> functions;
    memory::DynArray<std::string_view> strings;
    memory::DynArray<int64_t> constants;
    memory::DynArray<std::string_view> externs;

    explicit Module(memory::Arena &arena)
        : functions(arena), strings(arena), constants(arena), externs(arena) {}
};

} // namespace zith::ir
