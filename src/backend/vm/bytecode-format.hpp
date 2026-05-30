#pragma once

#include <cstdint>

namespace zith::backend::vm {

    enum class BytecodeOp : uint8_t {
        Nop,
        PushI64, PushF64, PushStr,
        Add, Sub, Mul, Div, Rem,
        Eq, Ne, Lt, Le, Gt, Ge,
        And, Or, Xor,
        Load, Store,
        Call, Ret,
        Jmp, Br,
        Halt
    };

    #pragma pack(push, 1)
    struct BytecodeHeader {
        uint32_t magic = 0x5A4954;
        uint32_t version = 1;
        uint32_t entry_point;
        uint32_t const_pool_offset;
        uint32_t code_offset;
    };

    struct BCInst {
        BytecodeOp op;
        uint8_t padding[3];
        int64_t operand;
    };
    #pragma pack(pop)

    static_assert(sizeof(BCInst) == 12, "BCInst must be 12 bytes");

}
