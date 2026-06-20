#pragma once

#include "memory/dyn-array.hpp"
#include "types/type-id.hpp"

#include <cstdint>
#include <string_view>
#include <variant>

namespace zith::zir {

using ZirIndex = uint32_t;
inline constexpr ZirIndex kInvalidIndex = ~ZirIndex{0};

enum class ZirOp : uint8_t {
    Halt,
    Push,
    Dup,
    Pop,
    Load,
    Store,
    AddI,
    SubI,
    MulI,
    DivI,
    CmpI,
    AddF,
    SubF,
    MulF,
    DivF,
    CmpF,
    Br,
    BrZ,
    BrGt,
    Call,
    Ret,
    Write,
    Input,
    CastI32,
    CastF64,
    Nop,
};

struct ZirInst {
    ZirOp op;
    uint32_t imm32 = 0;
    uint8_t imm8   = 0;

    ZirInst() : op(ZirOp::Nop) {}
    explicit ZirInst(ZirOp op, uint32_t imm32 = 0, uint8_t imm8 = 0)
        : op(op), imm32(imm32), imm8(imm8) {}
};

struct ZirBlock {
    memory::DynArray<ZirInst> code;

    explicit ZirBlock(memory::Arena &arena) : code(arena) {}
};

struct ZirFn {
    std::string_view name;
    memory::DynArray<ZirBlock> blocks;
    uint8_t param_count  = 0;
    uint8_t local_count   = 0;
    types::TypeId ret_type = types::kVoidType;

    explicit ZirFn(memory::Arena &arena) : blocks(arena) {}
};

struct ZirModule {
    memory::DynArray<ZirFn> functions;
    memory::DynArray<std::variant<int64_t, double, std::string>> constants;

    explicit ZirModule(memory::Arena &arena) : functions(arena), constants(arena) {}
};

} // namespace zith::zir
