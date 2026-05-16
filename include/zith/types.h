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
} ZithLiteralKind;

typedef enum ZithParserMode {
    ZITH_MODE_SCAN   = 0,
    ZITH_MODE_EXPAND = 1,
    ZITH_MODE_SEMA   = 2,
} ZithParserMode;

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
}
#endif
