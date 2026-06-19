#pragma once

#include "types/type-id.hpp"

#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

namespace zith::zir {

using ZirIndex = uint32_t;
inline constexpr ZirIndex kInvalidIndex = ~ZirIndex{0};

enum class ZirOp : uint8_t {
    Halt,
    Push,     // push immediate (u32 index into constants table)
    Dup,      // duplicate top of stack
    Pop,      // discard top of stack
    Load,     // load from frame slot (u8 slot index)
    Store,    // store to frame slot (u8 slot index)
    AddI,     // integer add
    SubI,
    MulI,
    DivI,
    CmpI,     // compare integers: push -1/0/1
    AddF,     // float add
    SubF,
    MulF,
    DivF,
    CmpF,
    Br,       // unconditional branch (i32 offset)
    BrZ,      // branch if zero (i32 offset)
    BrGt,     // branch if greater than zero
    Call,     // call function (u32 fn index, u8 arg count)
    Ret,      // return (u8 result count)
    Write,    // builtin: write(string)
    Input,    // builtin: read line, push string
    CastI32,  // cast to i32
    CastF64,  // cast to f64
    Nop,
};

struct ZirInst {
    ZirOp op;
    uint32_t imm32 = 0;
    uint8_t imm8   = 0;

    ZirInst() = default;
    explicit ZirInst(ZirOp op, uint32_t imm32 = 0, uint8_t imm8 = 0)
        : op(op), imm32(imm32), imm8(imm8) {}
};

struct ZirBlock {
    std::vector<ZirInst> code;
};

struct ZirFn {
    std::string_view name;
    std::vector<ZirBlock> blocks;
    uint8_t param_count  = 0;
    uint8_t local_count   = 0;
    types::TypeId ret_type = types::kVoidType;
};

struct ZirModule {
    std::vector<ZirFn> functions;
    std::vector<std::variant<int64_t, double, std::string>> constants;

    ZirModule() = default;
};

} // namespace zith::zir