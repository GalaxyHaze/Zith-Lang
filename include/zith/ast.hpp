#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "token.hpp"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t ZithNodeId;

enum {
    ZITH_NODE_ERROR = 0,
    ZITH_NODE_LITERAL = 100,
    ZITH_NODE_IDENTIFIER = 101,
    ZITH_NODE_BINARY_OP = 102,
    ZITH_NODE_UNARY_OP = 103,
    ZITH_NODE_CALL = 104,
    ZITH_NODE_INDEX = 105,
    ZITH_NODE_MEMBER = 106,
    ZITH_NODE_VAR_DECL = 200,
    ZITH_NODE_FUNC_DECL = 201,
    ZITH_NODE_PARAM = 202,
    ZITH_NODE_BLOCK = 300,
    ZITH_NODE_IF = 301,
    ZITH_NODE_FOR = 302,
    ZITH_NODE_RETURN = 303,
    ZITH_NODE_EXPR_STMT = 304,
    ZITH_NODE_UNBODY = 305,
    ZITH_NODE_TYPE_REF = 400,
    ZITH_NODE_TYPE_FUNC = 401,
    ZITH_NODE_CUSTOM_START = 1000
};

typedef struct ZithNode ZithNode;

struct ZithNode {
    union {
        struct { ZithNode *a; ZithNode *b; ZithNode *c; } kids;
        struct { void *ptr; size_t len; } list;
        struct { const char *str; size_t len; } ident;
        struct { double num; } number;
        struct { bool value; } boolean;
        uint64_t custom;
    } data;
    ZithSourceLoc loc;
    ZithNodeId type;
};

static inline ZithNodeId zith_node_type(const ZithNode *node) {
    return node ? node->type : (ZithNodeId) ZITH_NODE_ERROR;
}

#ifdef __cplusplus
}
#endif
