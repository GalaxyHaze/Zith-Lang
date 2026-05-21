#pragma once
#include "zith/types.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdint.h>
#include <stddef.h>
#include <string_view>
#include <type_traits>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

    // --- Core Types ---
    typedef uint8_t code_t;
    typedef uint8_t reg_t;

    // Runtime Value (matches register size)
    typedef union {
        int64_t     as_i64;
        uint64_t    as_u64;
        double      as_f64;
        void*       as_ptr;
        struct { uintptr_t low; uintptr_t high; } as_small;
    } ZithFFiValue;


    // --- FFI Definitions ---

    typedef enum {
        ZITH_FFI_VOID,
        ZITH_FFI_I64,
        ZITH_FFI_U64,
        ZITH_FFI_F64,
        ZITH_FFI_SMALL,
        ZITH_FFI_PTR
    } ZithFFIType;

    typedef struct {
        ZithFFIType ret_type;
        ZithFFIType* arg_types;
        uint8_t      arg_count;
    } ZithFFISignature;

    typedef struct {
        void*             func_ptr;
        ZithFFISignature  sig;
    } ZithFFIFunction;

    // --- Module & Function ---

    typedef struct {
        code_t*     code;
        size_t      code_size;
        char*       name;
    } ZithVmFunction;

    typedef struct {
        ZithVmFunction* fns;
        size_t          fn_count;
    } ZithVmModule;

    // --- Execution ---

    int ZithVmExecute(
        ZithVmModule* module,
        ZithFFIFunction* externs,
        size_t extern_len,
        code_t* body
    );

#ifdef __cplusplus
}
#endif

namespace Zith {

using Code = std::vector<uint8_t>;

// ============================================================================
// Register Convention (virtual registers r0–r255, all 64-bit)
// ============================================================================
//   r0  : zero register — always reads as 0, writes are ignored
//   r1  : stack pointer (SP)
//   r2  : frame pointer (FP)
//   r3  : link register (LR) — return address for CALL
//   r4  : thread base pointer (TP)
//   r5–r7 : reserved for runtime/platform
//   r8–r15 : argument / return registers (caller-saved)
//   r16–r31 : callee-saved registers
//   r32–r255 : free / temporary registers (caller-saved)

enum Register : reg_t {
    REG_ZERO = 0,    // hardwired zero
    REG_SP   = 1,    // stack pointer
    REG_FP   = 2,    // frame pointer
    REG_LR   = 3,    // link register (return address)
    REG_TP   = 4,    // thread base pointer
    REG_ARG0 = 8,    // first argument / return value
    REG_ARG1 = 9,
    REG_ARG2 = 10,
    REG_ARG3 = 11,
    REG_ARG4 = 12,
    REG_ARG5 = 13,
    REG_ARG6 = 14,
    REG_ARG7 = 15,
    REG_SAVED0 = 16, // first callee-saved
    REG_SAVED15 = 31, // last callee-saved
    REG_FREE0 = 32,  // first free/temporary
};

// ============================================================================
// Condition Codes (used by ICMP / FCMP)
// ============================================================================

enum CondCode : reg_t {
    // Signed integer
    CC_EQ  = 0,    // equal
    CC_NE  = 1,    // not equal
    CC_SLT = 2,    // signed less-than
    CC_SLE = 3,    // signed less-or-equal
    CC_SGT = 4,    // signed greater-than
    CC_SGE = 5,    // signed greater-or-equal
    // Unsigned integer
    CC_ULT = 6,    // unsigned less-than
    CC_ULE = 7,    // unsigned less-or-equal
    CC_UGT = 8,    // unsigned greater-than
    CC_UGE = 9,    // unsigned greater-or-equal
    // Float ordered
    CC_OEQ = 10,   // ordered equal
    CC_ONE = 11,   // ordered not equal
    CC_OLT = 12,   // ordered less-than
    CC_OLE = 13,   // ordered less-or-equal
    CC_OGT = 14,   // ordered greater-than
    CC_OGE = 15,   // ordered greater-or-equal
    // Float unordered
    CC_UEQ = 16,   // unordered equal
    CC_UNE = 17,   // unordered not equal
    CC_ULT = 18,   // unordered less-than
    CC_ULE = 19,   // unordered less-or-equal
    CC_UGT = 20,   // unordered greater-than
    CC_UGE = 21,   // unordered greater-or-equal
    CC_ORD = 22,   // ordered (no NaN)
    CC_UNO = 23,   // unordered (is NaN)
    CC_TRUE = 24,  // always true
    CC_FALSE = 25, // always false
};

// ============================================================================
// Instruction Set (.zbc Stable IR)
// ============================================================================
// Variable-width encoding. Each opcode determines its total byte length.
// A decode table maps opcode → { length, format } for fast interpretation.
//
// Operand shorthand:
//   r    = register byte  (reg_t,  1 byte)
//   i8   = 8-bit immediate (1 byte)
//   i32  = 32-bit immediate, little-endian (4 bytes)
//   i64  = 64-bit immediate, little-endian (8 bytes)
//   f64  = 64-bit IEEE-754 double, little-endian (8 bytes)
//   lbl  = 32-bit signed label offset, from end of instruction (4 bytes)
//   cond = condition code byte (CondCode, 1 byte)
//   n    = count byte (1 byte)

enum Instructions : uint8_t {

    // ===== Core Integer ALU (0–16) =====
    // Format: [op][dest][src1][src2] — 4 bytes
    ADD    = 0,     // dest = src1 + src2
    SUB    = 1,     // dest = src1 - src2
    MUL    = 2,     // dest = src1 * src2
    DIV_S  = 3,     // dest = src1 / src2  (signed)
    DIV_U  = 4,     // dest = src1 / src2  (unsigned)
    MOD_S  = 5,     // dest = src1 % src2  (signed)
    MOD_U  = 6,     // dest = src1 % src2  (unsigned)
    AND    = 7,     // dest = src1 & src2
    OR     = 8,     // dest = src1 | src2
    XOR    = 9,     // dest = src1 ^ src2
    SHL    = 10,    // dest = src1 << src2
    SAR    = 11,    // dest = src1 >> src2  (arithmetic, sign-extending)
    SHR    = 12,    // dest = src1 >> src2  (logical, zero-extending)
    ROL    = 13,    // dest = rotate-left(src1, src2)
    ROR    = 14,    // dest = rotate-right(src1, src2)
    MULH_S = 15,    // dest = high 64 bits of signed src1 * src2
    MULH_U = 16,    // dest = high 64 bits of unsigned src1 * src2

    // ===== Float ALU (17–22) =====
    // Format: [op][dest][src1][src2] — 4 bytes
    FADD   = 17,    // dest = src1 + src2  (double)
    FSUB   = 18,    // dest = src1 - src2
    FMUL   = 19,    // dest = src1 * src2
    FDIV   = 20,    // dest = src1 / src2
    FMIN   = 21,    // dest = min(src1, src2)
    FMAX   = 22,    // dest = max(src1, src2)

    // ===== Integer Unary (23–29) =====
    // Format: [op][dest][src] — 3 bytes
    NEG    = 23,    // dest = -src  (two's complement)
    NOT    = 24,    // dest = ~src  (bitwise complement)
    ABS    = 25,    // dest = |src|  (signed absolute)
    POPCNT = 26,    // dest = number of 1 bits in src
    CLZ    = 27,    // dest = count leading zeros
    CTZ    = 28,    // dest = count trailing zeros
    BYTESWAP = 29,  // dest = byte-reversed src

    // ===== Float Unary (30–37) =====
    // Format: [op][dest][src] — 3 bytes
    FNEG   = 30,    // dest = -src
    FABS   = 31,    // dest = |src|
    FSQRT  = 32,    // dest = sqrt(src)
    FFLOOR = 33,    // dest = floor(src)
    FCEIL  = 34,    // dest = ceil(src)
    FTRUNC = 35,    // dest = trunc(src)
    FROUND = 36,    // dest = round-to-nearest(src)
    FCLASS = 37,    // dest = bitmask classification of src

    // ===== Register Copy (38) =====
    MOV    = 38,    // [MOV][dest][src] — 3 bytes

    // ===== Four-Operand (39–40) =====
    // Format: [op][dest][src1][src2][src3] — 5 bytes
    FFMA   = 39,    // dest = src1*src2 + src3  (fused multiply-add)
    SELECT = 40,    // dest = src1 ? src2 : src3  (bitwise: if src1 != 0)

    // ===== Immediate Loads (41–44) =====
    MOV_I8   = 41,  // [MOV_I8][dest][i8] — 3 bytes; zero-extends to 64-bit
    MOV_I32  = 42,  // [MOV_I32][dest][i32] — 6 bytes; sign-extends to 64-bit
    MOV_I64  = 43,  // [MOV_I64][dest][i64] — 10 bytes
    MOV_F64  = 44,  // [MOV_F64][dest][f64] — 10 bytes; IEEE-754 double bits

    // ===== Stack Allocate (45) =====
    ALLOCA  = 45,   // [ALLOCA][dest][i32_size][i8_align] — 10 bytes

    // ===== Comparisons (46–47) =====
    // Format: [op][dest][cond][src1][src2] — 5 bytes
    ICMP   = 46,    // dest = src1 cond src2  (integer; result 0 or 1)
    FCMP   = 47,    // dest = src1 cond src2  (float; result 0 or 1)

    // ===== Shift by Immediate (48–50) =====
    // Format: [op][dest][src][i8] — 4 bytes
    SHL_I  = 48,    // dest = src << i8
    SAR_I  = 49,    // dest = src >> i8  (arithmetic)
    SHR_I  = 50,    // dest = src >> i8  (logical)

    // ===== GetElementPtr (51) =====
    GEP    = 51,    // [GEP][dest][ptr][index] — 4 bytes; dest = ptr + index*8

    // ===== Test & Freeze (52–53) =====
    TEST   = 52,    // [TEST][dest][src1][src2] — 4 bytes; dest = (src1 & src2) != 0
    FREEZE = 53,    // [FREEZE][dest][src] — 3 bytes; dest = src (freeze undef/poison)

    // ===== Memory Loads (54–57) =====
    // Format: [op][dest][addr] — 3 bytes
    LOAD64 = 54,    // dest = *(uint64_t*)addr
    LOAD32 = 55,    // dest = *(uint32_t*)addr  (zero-extended)
    LOAD16 = 56,    // dest = *(uint16_t*)addr  (zero-extended)
    LOAD8  = 57,    // dest = *(uint8_t*)addr   (zero-extended)

    // ===== Memory Stores (58–61) =====
    // Format: [op][addr][src] — 3 bytes
    STORE64 = 58,   // *(uint64_t*)addr = src
    STORE32 = 59,   // *(uint32_t*)addr = (uint32_t)src
    STORE16 = 60,   // *(uint16_t*)addr = (uint16_t)src
    STORE8  = 61,   // *(uint8_t*)addr  = (uint8_t)src

    // ===== Float Memory (62–63) =====
    // Format: [op][dest/addr][addr/src] — 3 bytes
    LOADF64 = 62,   // dest = *(double*)addr
    STOREF64 = 63,  // *(double*)addr = src

    // ===== Control Flow (64–75) =====

    // Unconditional branch
    BR         = 64,   // [BR][lbl] — 5 bytes; jump to label

    // Conditional branch
    BR_COND    = 65,   // [BR_COND][cond][lbl_true][lbl_false] — 10 bytes

    // Jump table
    BR_TABLE   = 66,   // [BR_TABLE][val][lbl_default][n][(i32,lbl)...] — variable

    // Return
    RET        = 67,   // [RET] — 1 byte  (void return)
                       // [RET][val] — 2 bytes (return with value in reg)

    // Indirect call (calls function pointer in register)
    CALL       = 68,   // [CALL][ret_reg/0xFF][func_reg][n][args...] — variable
                       // ret_reg=0xFF means no return value

    // Direct call (calls a label in the same module)
    CALL_DIRECT = 69,  // [CALL_DIRECT][ret_reg/0xFF][lbl][n][args...] — variable

    // Invoke (call with exception landing pad)
    INVOKE     = 70,   // [INVOKE][ret_reg/0xFF][func_reg][n][args...][lbl_normal][lbl_unwind]
                       // — variable

    // Landing pad (exception entry)
    LANDINGPAD = 71,   // [LANDINGPAD][dest] — 2 bytes

    // PHI node (SSA)
    PHI        = 72,   // [PHI][dest][n][(reg,lbl)...] — variable

    // Unreachable (must terminate a basic block)
    UNREACHABLE = 73,  // [UNREACHABLE] — 1 byte

    // External FFI call
    EXTERN     = 74,   // [EXTERN][func_id][ret_reg/0xFF][n][args...] — variable

    // Label marker (not executed; marks basic block start)
    LABEL      = 75,   // [LABEL][lbl_id] — 5 bytes

    // ===== Type Conversions (76–79) =====
    // Format: [op][dest][src] — 3 bytes
    I2F        = 76,   // dest = (double)src  (signed int to double)
    F2I        = 77,   // dest = (int64_t)src  (truncate double to signed int)
    F2I_ROUND  = 78,   // dest = (int64_t)round(src)  (round-to-nearest)
    BITCAST    = 79,   // dest = bitcast src  (reinterpret bits, same width)

    // ===== Extend / Truncate (80–83) =====
    // Format: [op][dest][src] — 3 bytes
    SEXT8  = 80,   // dest = sign-extend byte src to 64-bit
    SEXT16 = 81,   // dest = sign-extend 16-bit src to 64-bit
    ZEXT8  = 82,   // dest = zero-extend byte src to 64-bit
    ZEXT16 = 83,   // dest = zero-extend 16-bit src to 64-bit

    // ===== Pointer Conversions (84–85) =====
    // Format: [op][dest][src] — 3 bytes
    PTR2I = 84,    // dest = (uint64_t)src (pointer to integer)
    I2PTR = 85,    // dest = (void*)src    (integer to pointer)

    // ===== Memory Fences (86–89) =====
    // Format: [op][ordering] — 2 bytes
    FENCE_SEQ     = 86,  // sequentially consistent (mfence)
    FENCE_ACQ     = 87,  // acquire (lfence)
    FENCE_REL     = 88,  // release (sfence)
    FENCE_ACQ_REL = 89,  // acquire + release

    // ===== Atomics (90) =====
    // Format: [op][addr][cmp][new][ordering] — 5 bytes
    CMPXCHG = 90,   // *(uint64_t*)addr == cmp ? *(uint64_t*)addr = new : continue
                     // ordering: 0=SEQ_CST, 1=ACQUIRE, 2=RELEASE, 3=ACQ_REL, 4=RELAXED

    // ===== Misc System (91–94) =====
    NOP        = 91,  // [NOP] — 1 byte
    BREAKPOINT = 92,  // [BREAKPOINT] — 1 byte  (trap to debugger)
    YIELD      = 93,  // [YIELD] — 1 byte  (hint: yield thread/scheduler)
    SYSCALL    = 94,  // [SYSCALL][num] — 2 bytes

    // ===== Extension Space (95–254) =====
    // Open for vector ops, custom intrinsics, platform-specific extensions.
    EXT_START = 95,

    // ===== Halt (255) =====
    HALT = 255,      // [HALT] — 1 byte; terminates execution
};

// ============================================================================
// Instruction Decode Info
// ============================================================================
// For software VM decoding: total byte length and operand count per opcode.

struct InstrDecode {
    uint8_t length;    // total bytes (0 = variable, determined by operands)
    uint8_t n_operands;
};

// The decode table is indexed by opcode. InstrDecodeTable[op] gives format.
// (defined in the VM implementation)
extern const InstrDecode InstrDecodeTable[256];

// ============================================================================
// Extension Opcodes (used via EXT_START sub-encoding or after HALT escape)
// ============================================================================
// Debug/trace extensions can use opcodes EXT_START + n, or the HALT escape.
// For now, HALT escape: [HALT][ext_op][args...]

enum ExtOp : uint8_t {
    EXT_PRINT  = 0,   // print register value
    EXT_INPUT  = 1,   // read input into register
    EXT_OPEN   = 2,   // open file
    EXT_READ   = 3,   // read from file
    EXT_WRITE  = 4,   // write to file
    EXT_CLOSE  = 5,   // close file
    EXT_ASSERT = 6,   // assert (halt if register != 0)
};

} // namespace Zith
