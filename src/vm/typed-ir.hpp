#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"

#include <cstdint>
#include <string_view>

namespace zith::vm {

/// Typed value categories used by the v2 execution IR.
enum class ValueType : uint8_t {
    I32,
    I64,
    F32,
    F64,
    Ptr,
    Slice,
    FnRef,
    ExternRef,
    Void,
};

/// Instruction opcodes for the typed v2 runtime.
enum class Op : uint8_t {
    LoadConstI32,
    LoadConstI64,
    LoadConstF32,
    LoadConstF64,
    LoadString,
    LoadFnRef,
    LoadExternRef,
    AllocBytes,
    MallocBytes,
    StoreBytes,
    LoadBytes,
    FieldPtr,
    MakeSlice,
    SlicePtr,
    SliceLen,
    MemCopy,
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    Neg,
    Not,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    CallFn,
    CallFnRef,
    CallExtern,
    CallExternRef,
    Ret,
    Branch,
    Jump,
    Trap,
};

/// One v2 instruction row. `a/b/c/d/e` hold register or immediate indices
/// exactly as the opcode requires; `imm` holds a table index, jump target,
/// or a second indirect-call argument.
struct Instr {
    Op op        = Op::Trap;
    uint16_t a   = 0;
    uint16_t b   = 0;
    uint16_t c   = 0;
    uint16_t imm = 0;
    uint16_t d   = 0;
    uint16_t e   = 0;

    /// `0` is a valid guest offset, so allocation failures are distinguished
    /// separately by the readers of `LinearMemory`.

    [[nodiscard]] static auto trap() -> Instr {
        return Instr{};
    }

    [[nodiscard]] static auto simple(Op value, uint16_t dst, uint16_t lhs, uint16_t rhs = 0)
        -> Instr {
        return Instr{value, dst, lhs, rhs, 0};
    }

    [[nodiscard]] static auto withImm(Op value, uint16_t dst, uint16_t tableIndex) -> Instr {
        return Instr{value, dst, 0, 0, tableIndex};
    }

    [[nodiscard]] static auto callRef(Op value, uint16_t dst, uint16_t fnRef, uint16_t arg0,
                                      uint16_t arg1) -> Instr {
        return Instr{value, dst, fnRef, arg0, arg1};
    }

    [[nodiscard]] static auto callExtern(Op value, uint16_t dst, uint16_t arg0, uint16_t arg1,
                                         uint16_t tableIndex, uint16_t arg2 = 0, uint16_t arg3 = 0)
        -> Instr {
        Instr row;
        row.op  = value;
        row.a   = dst;
        row.b   = arg0;
        row.c   = arg1;
        row.d   = arg2;
        row.e   = arg3;
        row.imm = tableIndex;
        return row;
    }
};

struct Function {
    std::string_view name;
    ValueType returnType = ValueType::Void;
    uint16_t paramCount  = 0;
    uint16_t regCount    = 0;
    memory::DynArray<ValueType> regTypes;
    memory::DynArray<Instr> body;

    explicit Function(memory::Arena &arena) : regTypes(arena), body(arena) {}
};

struct Module {
    memory::DynArray<Function> functions;
    memory::DynArray<std::string_view> strings;
    memory::DynArray<int64_t> i64Constants;
    memory::DynArray<double> f64Constants;
    memory::DynArray<std::string_view> externs;

    explicit Module(memory::Arena &arena)
        : functions(arena), strings(arena), i64Constants(arena), f64Constants(arena),
          externs(arena) {}
};

} // namespace zith::vm
