#pragma once
#include <cstdint>
#include <vector>

using ZithCode = std::vector<uint8_t>;

// ============================================================================
// Opcode Enum — mirrors Zith::Instructions from vm.hpp
// ============================================================================

enum Op : uint8_t {
    // ===== Core Integer ALU (0–16) =====
    // Format: [op][dest][src1][src2] — 4 bytes
    ADD    = 0,
    SUB    = 1,
    MUL    = 2,
    DIV_S  = 3,
    DIV_U  = 4,
    MOD_S  = 5,
    MOD_U  = 6,
    AND    = 7,
    OR     = 8,
    XOR    = 9,
    SHL    = 10,
    SAR    = 11,
    SHR    = 12,
    ROL    = 13,
    ROR    = 14,
    MULH_S = 15,
    MULH_U = 16,

    // ===== Float ALU (17–22) =====
    FADD   = 17,
    FSUB   = 18,
    FMUL   = 19,
    FDIV   = 20,
    FMIN   = 21,
    FMAX   = 22,

    // ===== Integer Unary (23–29) =====
    // Format: [op][dest][src] — 3 bytes
    NEG    = 23,
    NOT    = 24,
    ABS    = 25,
    POPCNT = 26,
    CLZ    = 27,
    CTZ    = 28,
    BYTESWAP = 29,

    // ===== Float Unary (30–37) =====
    FNEG   = 30,
    FABS   = 31,
    FSQRT  = 32,
    FFLOOR = 33,
    FCEIL  = 34,
    FTRUNC = 35,
    FROUND = 36,
    FCLASS = 37,

    // ===== Register Copy (38) =====
    MOV    = 38,

    // ===== Four-Operand (39–40) =====
    // Format: [op][dest][src1][src2][src3] — 5 bytes
    FFMA   = 39,
    SELECT = 40,

    // ===== Immediate Loads (41–44) =====
    MOV_I8   = 41,
    MOV_I32  = 42,
    MOV_I64  = 43,
    MOV_F64  = 44,

    // ===== Stack Allocate (45) =====
    ALLOCA  = 45,

    // ===== Comparisons (46–47) =====
    // Format: [op][dest][cond][src1][src2] — 5 bytes
    ICMP   = 46,
    FCMP   = 47,

    // ===== Shift by Immediate (48–50) =====
    // Format: [op][dest][src][i8] — 4 bytes
    SHL_I  = 48,
    SAR_I  = 49,
    SHR_I  = 50,

    // ===== GetElementPtr (51) =====
    GEP    = 51,

    // ===== Test & Freeze (52–53) =====
    TEST   = 52,
    FREEZE = 53,

    // ===== Memory Loads (54–57) =====
    LOAD64 = 54,
    LOAD32 = 55,
    LOAD16 = 56,
    LOAD8  = 57,

    // ===== Memory Stores (58–61) =====
    STORE64 = 58,
    STORE32 = 59,
    STORE16 = 60,
    STORE8  = 61,

    // ===== Float Memory (62–63) =====
    LOADF64  = 62,
    STOREF64 = 63,

    // ===== Control Flow (64–75) =====
    BR         = 64,
    BR_COND    = 65,
    BR_TABLE   = 66,
    RET        = 67,
    CALL       = 68,
    CALL_DIRECT = 69,
    INVOKE     = 70,
    LANDINGPAD = 71,
    PHI        = 72,
    UNREACHABLE = 73,
    EXTERN     = 74,
    LABEL      = 75,

    // ===== Type Conversions (76–79) =====
    I2F        = 76,
    F2I        = 77,
    F2I_ROUND  = 78,
    BITCAST    = 79,

    // ===== Extend / Truncate (80–83) =====
    SEXT8  = 80,
    SEXT16 = 81,
    ZEXT8  = 82,
    ZEXT16 = 83,

    // ===== Pointer Conversions (84–85) =====
    PTR2I = 84,
    I2PTR = 85,

    // ===== Memory Fences (86–89) =====
    FENCE_SEQ     = 86,
    FENCE_ACQ     = 87,
    FENCE_REL     = 88,
    FENCE_ACQ_REL = 89,

    // ===== Atomics (90) =====
    CMPXCHG = 90,

    // ===== Misc System (91–94) =====
    NOP        = 91,
    BREAKPOINT = 92,
    YIELD      = 93,
    SYSCALL    = 94,

    // ===== Extension Space (95–254) =====
    EXT_START = 95,

    // ===== Halt (255) =====
    HALT = 255,
};

// Extension set for debug (used when opcode = HALT in debug mode)
enum ExtOp : uint32_t {
    EXT_PRINT  = 0,
    EXT_INPUT  = 1,
    EXT_ASSERT = 2,
    EXT_TRACE  = 3,
    EXT_BREAKPOINT = 4,
};
