// include/zith/vm.hpp — Zith Binary Code (ZBC) v2: SSA-based, LLVM-round-trippable IR
#pragma once
#include "zith/types.h"
#include "zith/leb128.h"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

// ============================================================================
// ZBC Binary Format Layout
// ============================================================================
//
// [Header]        "ZBC\0" + version (u8) + flags (u8)
// [TypeTable]     count (uleb) + TypeEntry...
// [StringTable]   count (uleb) + StringEntry...  (offset + length pairs + data)
// [Globals]       count (uleb) + GlobalEntry...
// [Functions]     count (uleb) + FunctionEntry...
//
// All ValueIds are unsigned LEB128 encoded.
// Instructions are grouped by BasicBlock within Functions.
// Each BasicBlock ends with exactly one terminator instruction.

namespace Zith {

// ============================================================================
// Magic and Version
// ============================================================================

static constexpr uint8_t ZBC_MAGIC[4] = {'Z', 'B', 'C', '\0'};
static constexpr uint8_t ZBC_VERSION   = 2;

enum ZbcFlags : uint8_t {
    ZBC_FLAG_NONE      = 0,
    ZBC_FLAG_OPTIMIZED = 1 << 0,
    ZBC_FLAG_DEBUG     = 1 << 1,
};

// ============================================================================
// Opcodes (first byte of instruction)
// ============================================================================
// Second byte is always a ZithTypeId specifying the result type.
// Exception: terminators and control-flow opcodes that don't produce values.

enum Opcode : uint8_t {
    // --- Integer ALU (result type byte follows) ---
    OP_ADD   = 0x00,
    OP_SUB   = 0x01,
    OP_MUL   = 0x02,
    OP_SDIV  = 0x03,
    OP_UDIV  = 0x04,
    OP_SREM  = 0x05,
    OP_UREM  = 0x06,
    OP_AND   = 0x07,
    OP_OR    = 0x08,
    OP_XOR   = 0x09,
    OP_SHL   = 0x0A,
    OP_ASHR  = 0x0B,
    OP_LSHR  = 0x0C,

    // --- Float ALU ---
    OP_FADD  = 0x10,
    OP_FSUB  = 0x11,
    OP_FMUL  = 0x12,
    OP_FDIV  = 0x13,
    OP_FREM  = 0x14,

    // --- Comparisons (result type is always i1) ---
    OP_ICMP  = 0x20,
    OP_FCMP  = 0x21,

    // --- Memory ---
    OP_LOAD   = 0x30,
    OP_STORE  = 0x31,
    OP_ALLOCA = 0x32,
    OP_GEP    = 0x33,

    // --- Type Conversions ---
    OP_TRUNC    = 0x40,
    OP_ZEXT     = 0x41,
    OP_SEXT     = 0x42,
    OP_FPTRUNC  = 0x43,
    OP_FPEXT    = 0x44,
    OP_FPTOUI   = 0x45,
    OP_FPTOSI   = 0x46,
    OP_UITOFP   = 0x47,
    OP_SITOFP   = 0x48,
    OP_PTR2INT  = 0x49,
    OP_INT2PTR  = 0x4A,
    OP_BITCAST  = 0x4B,
    OP_INTTOPTR = 0x4C,
    OP_PTRTOINT = 0x4D,

    // --- SSA ---
    OP_PHI    = 0x60,
    OP_FREEZE = 0x61,

    // --- Calls ---
    OP_CALL          = 0x70,
    OP_CALL_INDIRECT = 0x71,

    // --- Atomics ---
    OP_ATOMICRMW = 0x80,
    OP_CMPXCHG   = 0x81,
    OP_FENCE     = 0x82,

    // --- Aggregate ---
    OP_EXTRACTVALUE   = 0x90,
    OP_INSERTVALUE    = 0x91,
    OP_EXTRACTELEMENT = 0x92,
    OP_INSERTELEMENT  = 0x93,
    OP_SHUFFLEVECTOR  = 0x94,

    // --- Intrinsics ---
    OP_INTRINSIC = 0xF0,

    // --- Constants (define a new ValueId with a constant value) ---
    OP_CONST_INT   = 0xE0,
    OP_CONST_FLOAT = 0xE1,
    OP_CONST_PTR   = 0xE2,
    OP_CONST_NULL  = 0xE3,
    OP_CONST_AGG   = 0xE4,
    OP_CONST_STRING = 0xE5,

    // --- Terminators (no type byte, no result ValueId) ---
    OP_BR         = 0x50,
    OP_COND_BR    = 0x51,
    OP_SWITCH     = 0x52,
    OP_RET        = 0x53,
    OP_UNREACHABLE = 0x54,

    // --- Misc ---
    OP_SELECT = 0xA0,
};

// ============================================================================
// Instruction Encoding Reference
// ============================================================================
//
// All ValueId references are unsigned LEB128.
// BlockId references are unsigned LEB128 (index into function's block list).
//
// Binary ALU (OP_ADD, OP_SUB, ...):
//   [opcode][type_byte][overflow_flags][result_vid][lhs_vid][rhs_vid]
//   overflow_flags: 0=none, 1=nsw, 2=nuw, 3=nsw|nuw
//
// Float ALU (OP_FADD, OP_FSUB, ...):
//   [opcode][type_byte][result_vid][lhs_vid][rhs_vid]
//
// Comparisons (OP_ICMP, OP_FCMP):
//   [opcode][ZITH_TYPE_I1][result_vid][pred_byte][lhs_vid][rhs_vid]
//   pred_byte: ZithICmpPred or ZithFCmpPred
//
// Load (OP_LOAD):
//   [opcode][type_byte][result_vid][addr_vid][align_log2][ordering]
//   align_log2: alignment as log2 (e.g., 3 for align 8)
//   ordering: ZithMemoryOrdering
//
// Store (OP_STORE):
//   [opcode][type_byte][value_vid][addr_vid][align_log2][ordering]
//
// Alloca (OP_ALLOCA):
//   [opcode][type_byte][result_vid][alloc_type_id][align_log2]
//
// GEP (OP_GEP):
//   [opcode][ZITH_TYPE_PTR][result_vid][base_ptr_vid][n_indices (uleb)]
//     [idx_type_0][idx_vid_0] ... [idx_type_n-1][idx_vid_n-1]
//
// Conversions (OP_TRUNC, OP_ZEXT, ...):
//   [opcode][result_type_byte][result_vid][src_vid]
//   For OP_TRUNC/OP_ZEXT/OP_SEXT: src type is inferred from context or
//   encoded as [opcode][result_type][src_type][result_vid][src_vid]
//
// PHI (OP_PHI):
//   [opcode][type_byte][result_vid][n_incoming (uleb)]
//     [vid_0][block_id_0] ... [vid_n-1][block_id_n-1]
//
// FREEZE (OP_FREEZE):
//   [opcode][type_byte][result_vid][src_vid]
//
// Call (OP_CALL):
//   [opcode][result_type_byte][result_vid][fn_vid][n_args (uleb)]
//     [arg_type_0][arg_vid_0] ... [arg_type_n-1][arg_vid_n-1]
//
// Call Indirect (OP_CALL_INDIRECT):
//   [opcode][result_type_byte][result_vid][fn_type_id][fn_ptr_vid][n_args (uleb)]
//     [arg_type_0][arg_vid_0] ...
//
// Intrinsic (OP_INTRINSIC):
//   [opcode][result_type_byte][result_vid][intrinsic_id_string_id][n_args (uleb)]
//     [arg_type_0][arg_vid_0] ...
//
// AtomicRMW (OP_ATOMICRMW):
//   [opcode][type_byte][result_vid][op_byte][addr_vid][value_vid][ordering]
//
// CmpXchg (OP_CMPXCHG):
//   [opcode][type_byte][result_vid][addr_vid][cmp_vid][new_vid][success_ordering][failure_ordering]
//   result is {type, i1} struct
//
// Fence (OP_FENCE):
//   [opcode][ordering]
//
// Select (OP_SELECT):
//   [opcode][type_byte][result_vid][cond_vid][true_vid][false_vid]
//
// ExtractValue (OP_EXTRACTVALUE):
//   [opcode][result_type][result_vid][agg_vid][n_indices (uleb)][idx_0]...[idx_n-1]
//
// InsertValue (OP_INSERTVALUE):
//   [opcode][result_type][result_vid][agg_vid][value_vid][n_indices (uleb)][idx_0]...[idx_n-1]
//
// Constants:
//   OP_CONST_INT:   [opcode][type_byte][result_vid][value (sleb128 for signed, uleb128 for unsigned)]
//   OP_CONST_FLOAT: [opcode][type_byte][result_vid][f64_bits (8 bytes LE)]
//   OP_CONST_PTR:   [opcode][ZITH_TYPE_PTR][result_vid][address (uleb128)]
//   OP_CONST_NULL:  [opcode][type_byte][result_vid]
//   OP_CONST_AGG:   [opcode][type_byte][result_vid][n_elements (uleb)][vid_0]...[vid_n-1]
//   OP_CONST_STRING:[opcode][ZITH_TYPE_ARRAY][result_vid][string_id]
//
// Terminators:
//   OP_BR:          [opcode][target_block_id]
//   OP_COND_BR:     [opcode][cond_vid][true_block_id][false_block_id]
//   OP_SWITCH:      [opcode][value_vid][default_block_id][n_cases (uleb)]
//                     [case_value (sleb)][target_block_id]...
//   OP_RET:         [opcode][n_return_values (uleb)] [ret_type_0][ret_vid_0] ...
//   OP_UNREACHABLE: [opcode]

// ============================================================================
// Type Table Entry
// ============================================================================

struct TypeEntry {
    ZithTypeId base;
    // Compound type data (variable, encoded after base type in binary):
    //   ZITH_TYPE_FUNC:    { ret_type_id (uleb), n_params (uleb), param_type_ids[n] }
    //   ZITH_TYPE_STRUCT:  { name_string_id (uleb), n_fields (uleb), field_type_ids[n], field_name_ids[n] }
    //   ZITH_TYPE_ARRAY:   { element_type_id (uleb), count (uleb) }
    //   ZITH_TYPE_VECTOR:  { element_type_id (uleb), count (uleb) }
    //   ZITH_TYPE_PTR:     { pointee_type_id (uleb) }
};

// ============================================================================
// String Table Entry
// ============================================================================

struct StringEntry {
    uint32_t offset;  // offset into string blob
    uint32_t length;
};

// ============================================================================
// Global Variable Entry
// ============================================================================

struct GlobalEntry {
    uint32_t name_string_id;
    uint32_t type_id;           // index into TypeTable
    ZithLinkage linkage;
    uint8_t  align_log2;        // alignment as log2 (0 = unspecified)
    uint8_t  flags;             // bitfield: 1=constant, 2=tls, 4=unnamed_addr
    // initializer: either a ValueId reference or raw bytes
    uint32_t init_value_id;     // 0 = zero-initialized
};

// ============================================================================
// Function Entry
// ============================================================================

struct FunctionEntry {
    uint32_t name_string_id;
    uint32_t type_id;           // index into TypeTable (function type)
    ZithLinkage linkage;
    uint32_t attr_flags;        // ZithFnAttr bitfield
    ZithCallingConv calling_conv;
    uint32_t n_params;          // number of parameters (for SSA entry values)
    uint32_t n_blocks;          // number of basic blocks
    // Basic blocks follow sequentially in binary
};

// ============================================================================
// Basic Block
// ============================================================================

struct BasicBlock {
    uint32_t block_id;          // unique within function (0 = entry block)
    uint32_t n_phis;            // number of PHI nodes
    // PHI nodes encoded first, then regular instructions, then terminator
    // Terminator is always the last instruction in the block
};

// ============================================================================
// Module Header
// ============================================================================

struct ModuleHeader {
    uint8_t  magic[4];          // "ZBC\0"
    uint8_t  version;           // ZBC_VERSION
    uint8_t  flags;             // ZbcFlags
};

// ============================================================================
// Decoded Module (in-memory representation)
// ============================================================================

struct DecodedModule {
    std::vector<TypeEntry>      types;
    std::vector<std::string_view> strings;  // views into string blob
    std::vector<GlobalEntry>    globals;
    std::vector<FunctionEntry>  functions;
    // Raw bytecode for each function (decoded on demand)
    const uint8_t* function_data = nullptr;
    size_t         function_data_size = 0;
};

// ============================================================================
// Encode / Decode API
// ============================================================================

// Encode a module to binary. Returns the number of bytes written.
// If out_buf is nullptr, returns the required buffer size.
size_t ZbcEncodeModule(const DecodedModule* module, uint8_t* out_buf, size_t buf_size);

// Decode a module from binary. Returns 0 on success, -1 on error.
int ZbcDecodeModule(const uint8_t* data, size_t size, DecodedModule* out);

// Encode a single function's bytecode.
size_t ZbcEncodeFunction(const FunctionEntry* fn, const uint8_t* block_data,
                         size_t block_data_size, uint8_t* out_buf, size_t buf_size);

// Decode a single function's bytecode.
int ZbcDecodeFunction(const uint8_t* data, size_t size, FunctionEntry* out_fn,
                      const uint8_t** out_block_data, size_t* out_block_data_size);

} // namespace Zith
