// include/zith/types.h — Core types and enums for Zith (C ABI)
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZithArena ZithArena;

// ============================================================================
// AST Node Types — base node IDs (< 1000)
// ============================================================================

typedef enum ZithNodeId {
    ZITH_NODE_ERROR       = 0,
    ZITH_NODE_LITERAL     = 1,
    ZITH_NODE_IDENTIFIER  = 2,
    ZITH_NODE_BINARY_OP   = 3,
    ZITH_NODE_UNARY_OP    = 4,
    ZITH_NODE_CALL        = 5,
    ZITH_NODE_INDEX       = 6,
    ZITH_NODE_MEMBER      = 7,
    ZITH_NODE_VAR_DECL    = 8,
    ZITH_NODE_FUNC_DECL   = 9,
    ZITH_NODE_PARAM       = 10,
    ZITH_NODE_BLOCK       = 11,
    ZITH_NODE_IF          = 12,
    ZITH_NODE_FOR         = 13,
    ZITH_NODE_RETURN      = 14,
    ZITH_NODE_EXPR_STMT   = 15,
    ZITH_NODE_TYPE_REF    = 16,
    ZITH_NODE_TYPE_FUNC   = 17,
    ZITH_NODE_UNBODY      = 18,
} ZithNodeId;

typedef struct ZithNode ZithNode;

enum ZithNodeExtendedId : uint16_t {
    ZITH_NODE_ARROW_CALL    = 1000,
    ZITH_NODE_CAST          = 1001,
    ZITH_NODE_OPTIONAL_EXPR = 1002,
    ZITH_NODE_UNWRAP        = 1003,
    ZITH_NODE_RANGE         = 1004,
    ZITH_NODE_LAMBDA        = 1005,
    ZITH_NODE_SPAWN_EXPR    = 1006,
    ZITH_NODE_RECURSE       = 1007,

    ZITH_NODE_ARRAY_LIT     = 1020,
    ZITH_NODE_STRUCT_LIT    = 1021,
    ZITH_NODE_TUPLE_LIT     = 1022,

    ZITH_NODE_PROGRAM       = 1030,
    ZITH_NODE_CONST_DECL    = 1031,
    ZITH_NODE_STRUCT_DECL   = 1032,
    ZITH_NODE_ENUM_DECL     = 1033,
    ZITH_NODE_TRAIT_DECL    = 1034,
    ZITH_NODE_IMPL_DECL     = 1035,
    ZITH_NODE_TYPE_ALIAS    = 1036,
    ZITH_NODE_COMPONENT_DECL = 1037,
    ZITH_NODE_UNION_DECL    = 1038,
    ZITH_NODE_FAMILY_DECL   = 1039,
    ZITH_NODE_ENTITY_DECL   = 1040,
    ZITH_NODE_MODULE_DECL   = 1041,
    ZITH_NODE_IMPORT        = 1042,
    ZITH_NODE_EXPORT        = 1043,

    ZITH_NODE_SWITCH        = 1050,
    ZITH_NODE_CASE          = 1051,
    ZITH_NODE_BREAK         = 1052,
    ZITH_NODE_CONTINUE      = 1053,
    ZITH_NODE_GOTO          = 1054,
    ZITH_NODE_MARKER        = 1055,
    ZITH_NODE_ENTRY         = 1056,
    ZITH_NODE_SCENE         = 1057,
    ZITH_NODE_TRY_CATCH     = 1058,
    ZITH_NODE_SPAWN_STMT    = 1059,
    ZITH_NODE_AWAIT_STMT    = 1060,
    ZITH_NODE_YIELD         = 1061,
    ZITH_NODE_JOINED        = 1062,

    ZITH_NODE_TYPE_OPTIONAL = 1070,
    ZITH_NODE_TYPE_RESULT   = 1071,
    ZITH_NODE_TYPE_ARRAY    = 1072,
    ZITH_NODE_TYPE_SLICE    = 1084,
    ZITH_NODE_TYPE_TUPLE    = 1073,
    ZITH_NODE_TYPE_POINTER  = 1074,
    ZITH_NODE_TYPE_UNIQUE   = 1075,
    ZITH_NODE_TYPE_SHARED   = 1076,
    ZITH_NODE_TYPE_VIEW     = 1077,
    ZITH_NODE_TYPE_LEND     = 1078,
    ZITH_NODE_TYPE_PACK     = 1079,
    ZITH_NODE_TYPE_EXTENSION = 1080,

    ZITH_NODE_DESTRUCTURE   = 1081,

    ZITH_NODE_FIELD         = 1090,
    ZITH_NODE_ENUM_VARIANT  = 1091,
    ZITH_NODE_MATCH_ARM     = 1092,
    ZITH_NODE_CALL_ARG      = 1093,
    ZITH_NODE_STRUCT_LIT_FIELD = 1094,
};

typedef enum ZithFnKind {
    ZITH_FN_NORMAL   = 0,
    ZITH_FN_ASYNC    = 1,
    ZITH_FN_NORETURN = 2,
    ZITH_FN_FLOWING  = 3,
    ZITH_FN_RAW      = 4,
    ZITH_FN_CONST    = 5,
    ZITH_FN_COMPTIME = 6,
} ZithFnKind;

typedef enum ZithBindingKind {
    ZITH_BINDING_LET        = 0,
    ZITH_BINDING_VAR        = 1,
    ZITH_BINDING_CONST      = 2,
    ZITH_BINDING_AUTO       = 3,
    ZITH_BINDING_GLOBAL     = 4,
    ZITH_BINDING_PERSISTENT = 5,
    ZITH_BINDING_LOCAL      = 6,
    ZITH_BINDING_COMPTIME   = 7,
} ZithBindingKind;

typedef enum ZithOwnership {
    ZITH_OWN_DEFAULT  = 0,
    ZITH_OWN_VIEW     = 1,
    ZITH_OWN_LEND     = 2,
    ZITH_OWN_UNIQUE   = 3,
    ZITH_OWN_SHARED   = 4,
    ZITH_OWN_EXTENSION = 5,
} ZithOwnership;

typedef enum ZithVisibility {
    ZITH_VIS_PRIVATE = 0,
    ZITH_VIS_PUBLIC  = 1,
    ZITH_VIS_MODULE  = 2,
} ZithVisibility;

typedef enum ZithLiteralKind {
    ZITH_LIT_INT    = 0,
    ZITH_LIT_UINT   = 1,
    ZITH_LIT_FLOAT  = 2,
    ZITH_LIT_STRING = 3,
    ZITH_LIT_BOOL   = 4,
    ZITH_LIT_CHAR   = 5,
    ZITH_LIT_NULL   = 6,
} ZithLiteralKind;

typedef enum ZithParserMode {
    ZITH_MODE_SCAN   = 0,
    ZITH_MODE_EXPAND = 1,
    ZITH_MODE_SEMA   = 2,
} ZithParserMode;

// ============================================================================
// Zith Binary Code (ZBC) — LLVM-round-trippable IR types
// ============================================================================

// Value ID: references SSA values defined by prior instructions.
// Encoded as unsigned LEB128 in the binary format.
typedef uint32_t ZithValueId;

// Type IDs matching LLVM's type system for lossless round-trip.
typedef enum ZithTypeId {
    ZITH_TYPE_VOID    = 0,
    ZITH_TYPE_I1      = 1,
    ZITH_TYPE_I8      = 2,
    ZITH_TYPE_I16     = 3,
    ZITH_TYPE_I32     = 4,
    ZITH_TYPE_I64     = 5,
    ZITH_TYPE_I128    = 6,
    ZITH_TYPE_F16     = 7,
    ZITH_TYPE_F32     = 8,
    ZITH_TYPE_F64     = 9,
    ZITH_TYPE_PTR     = 10,
    ZITH_TYPE_FUNC    = 11,
    ZITH_TYPE_STRUCT  = 12,
    ZITH_TYPE_ARRAY   = 13,
    ZITH_TYPE_VECTOR  = 14,
} ZithTypeId;

// Overflow flags for integer arithmetic (matches LLVM nsw/nuw/exact).
typedef enum ZithOverflowFlags {
    ZITH_OVERFLOW_NONE    = 0,
    ZITH_OVERFLOW_NSW     = 1 << 0,
    ZITH_OVERFLOW_NUW     = 1 << 1,
    ZITH_OVERFLOW_EXACT   = 1 << 2,
} ZithOverflowFlags;

// Integer comparison predicates (matches LLVM ICMP).
typedef enum ZithICmpPred {
    ZITH_ICMP_EQ  = 0,
    ZITH_ICMP_NE  = 1,
    ZITH_ICMP_SLT = 2,
    ZITH_ICMP_SLE = 3,
    ZITH_ICMP_SGT = 4,
    ZITH_ICMP_SGE = 5,
    ZITH_ICMP_ULT = 6,
    ZITH_ICMP_ULE = 7,
    ZITH_ICMP_UGT = 8,
    ZITH_ICMP_UGE = 9,
} ZithICmpPred;

// Floating-point comparison predicates (matches LLVM FCMP).
typedef enum ZithFCmpPred {
    ZITH_FCMP_FALSE = 0,
    ZITH_FCMP_OEQ   = 1,
    ZITH_FCMP_OGT   = 2,
    ZITH_FCMP_OGE   = 3,
    ZITH_FCMP_OLT   = 4,
    ZITH_FCMP_OLE   = 5,
    ZITH_FCMP_ONE   = 6,
    ZITH_FCMP_ORD   = 7,
    ZITH_FCMP_UNO   = 8,
    ZITH_FCMP_UEQ   = 9,
    ZITH_FCMP_UGT   = 10,
    ZITH_FCMP_UGE   = 11,
    ZITH_FCMP_ULT   = 12,
    ZITH_FCMP_ULE   = 13,
    ZITH_FCMP_UNE   = 14,
    ZITH_FCMP_TRUE  = 15,
} ZithFCmpPred;

// Memory ordering for atomic operations (matches LLVM atomic ordering).
typedef enum ZithMemoryOrdering {
    ZITH_ORDER_UNORDERED  = 0,
    ZITH_ORDER_MONOTONIC  = 1,
    ZITH_ORDER_ACQUIRE    = 2,
    ZITH_ORDER_RELEASE    = 3,
    ZITH_ORDER_ACQ_REL    = 4,
    ZITH_ORDER_SEQ_CST    = 5,
} ZithMemoryOrdering;

// Atomic read-modify-write operations.
typedef enum ZithAtomicRMWOp {
    ZITH_RMW_XCHG   = 0,
    ZITH_RMW_ADD    = 1,
    ZITH_RMW_SUB    = 2,
    ZITH_RMW_AND    = 3,
    ZITH_RMW_NAND   = 4,
    ZITH_RMW_OR     = 5,
    ZITH_RMW_XOR    = 6,
    ZITH_RMW_MAX    = 7,
    ZITH_RMW_MIN    = 8,
    ZITH_RMW_UMAX   = 9,
    ZITH_RMW_UMIN   = 10,
    ZITH_RMW_FADD   = 11,
    ZITH_RMW_FSUB   = 12,
} ZithAtomicRMWOp;

// Linkage types for globals and functions (matches LLVM linkage).
typedef enum ZithLinkage {
    ZITH_LINKAGE_PRIVATE              = 0,
    ZITH_LINKAGE_INTERNAL             = 1,
    ZITH_LINKAGE_AVAILABLE_EXTERNALLY = 2,
    ZITH_LINKAGE_LINKONCE             = 3,
    ZITH_LINKAGE_WEAK                 = 4,
    ZITH_LINKAGE_COMMON               = 5,
    ZITH_LINKAGE_APPENDING            = 6,
    ZITH_LINKAGE_EXTERN_WEAK          = 7,
    ZITH_LINKAGE_EXTERNAL             = 8,
    ZITH_LINKAGE_LINKONCE_ODR         = 9,
    ZITH_LINKAGE_WEAK_ODR             = 10,
} ZithLinkage;

// Function attributes (bitfield).
typedef enum ZithFnAttr {
    ZITH_FN_ATTR_NONE       = 0,
    ZITH_FN_ATTR_NOUNWIND   = 1 << 0,
    ZITH_FN_ATTR_READONLY   = 1 << 1,
    ZITH_FN_ATTR_READNONE   = 1 << 2,
    ZITH_FN_ATTR_WRITEONLY  = 1 << 3,
    ZITH_FN_ATTR_ARGMEMONLY = 1 << 4,
    ZITH_FN_ATTR_NORETURN   = 1 << 5,
    ZITH_FN_ATTR_NOINLINE   = 1 << 6,
    ZITH_FN_ATTR_ALWAYSINLINE = 1 << 7,
    ZITH_FN_ATTR_NORECURSE  = 1 << 8,
    ZITH_FN_ATTR_CONVERGENT = 1 << 9,
} ZithFnAttr;

// Calling convention.
typedef enum ZithCallingConv {
    ZITH_CC_C         = 0,
    ZITH_CC_FAST      = 1,
    ZITH_CC_COLD      = 2,
    ZITH_CC_GHC       = 3,
    ZITH_CC_HIPE      = 4,
    ZITH_CC_ANY       = 5,
    ZITH_CC_SWIFT     = 6,
    ZITH_CC_PRESERVE_ALL = 7,
    ZITH_CC_X86_64_SYSV = 8,
    ZITH_CC_X86_64_WIN64 = 9,
} ZithCallingConv;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace zith {
    using FnKind      = ZithFnKind;
    using BindingKind = ZithBindingKind;
    using Ownership   = ZithOwnership;
    using Visibility  = ZithVisibility;
    using LiteralKind = ZithLiteralKind;
    using ParserMode  = ZithParserMode;
    using NodeExtendedId = ZithNodeExtendedId;

    // ZBC (Zith Binary Code) types
    using ValueId       = ZithValueId;
    using TypeId        = ZithTypeId;
    using OverflowFlags = ZithOverflowFlags;
    using ICmpPred      = ZithICmpPred;
    using FCmpPred      = ZithFCmpPred;
    using MemoryOrdering = ZithMemoryOrdering;
    using AtomicRMWOp   = ZithAtomicRMWOp;
    using Linkage       = ZithLinkage;
    using FnAttr        = ZithFnAttr;
    using CallingConv   = ZithCallingConv;
}
#endif
