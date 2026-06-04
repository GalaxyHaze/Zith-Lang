// include/zith/ast.h — Extended AST node IDs, payloads, constructors, and visitors (C ABI)
#pragma once

#include "zith/types.h"
#include "zith/token.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZithNodeKids {
    ZithNode *a;
    ZithNode *b;
    ZithNode *c;
} ZithNodeKids;

typedef struct ZithNodeList {
    void *ptr;
    size_t len;
} ZithNodeList;

typedef struct ZithNodeIdent {
    const char *str;
    size_t len;
} ZithNodeIdent;

typedef struct ZithNodeData {
    union {
        ZithNodeKids kids;
        ZithNodeList list;
        ZithNodeIdent ident;
        double number;
        bool boolean;
        uint64_t custom;
    };
} ZithNodeData;

struct ZithNode {
    uint16_t type;
    ZithSourceLoc loc;
    ZithNodeData data;
};

typedef struct {
    const char *name;
    size_t name_len;
    ZithFnKind kind;
    ZithNode **params;
    size_t param_count;
    ZithNode *return_type;
    ZithNode *body;
    ZithVisibility visibility;
    int32_t vis_depth;
    bool is_extern;
} ZithFuncPayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithBindingKind binding;
    ZithOwnership ownership;
    ZithVisibility visibility;
    int32_t vis_depth;
    ZithNode *type_node;
    ZithNode *initializer;
} ZithVarPayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithOwnership ownership;
    ZithNode *type_node;
    ZithNode *default_value;
    bool is_mutable;
} ZithParamPayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithOwnership ownership;
    ZithVisibility visibility;
    int32_t vis_depth;
    ZithNode *type_node;
    ZithNode *default_value;
} ZithFieldPayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithNode **fields;
    size_t field_count;
    ZithNode **methods;
    size_t method_count;
    ZithVisibility visibility;
    int32_t vis_depth;
} ZithStructPayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithNode **variants;
    size_t variant_count;
    ZithVisibility visibility;
    int32_t vis_depth;
} ZithEnumPayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithNode **types;
    size_t type_count;
    ZithVisibility visibility;
    int32_t vis_depth;
    bool is_raw;
} ZithUnionPayload;

typedef struct {
    ZithNode *subject;
    ZithNode **arms;
    size_t arm_count;
    ZithNode *default_arm;
} ZithSwitchPayload;

typedef struct {
    ZithNode *try_block;
    const char *catch_var;
    size_t catch_var_len;
    ZithNode *catch_block;
} ZithTryCatchPayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithNode *value;
} ZithEnumVariantPayload;

typedef struct {
    const char *target;
    size_t target_len;
    ZithNode **args;
    size_t arg_count;
    bool is_scene;
} ZithGotoPayload;

typedef struct {
    ZithNode *callee;
    ZithNode **args;
    size_t arg_count;
} ZithCallPayload;

typedef struct {
    ZithOwnership ownership;
    ZithNode *expr;
} ZithCallArgPayload;

typedef struct {
    ZithNode *iterator_var;
    ZithNode *iterable;
    ZithNode *init;
    ZithNode *condition;
    ZithNode *step;
    ZithNode *body;
    bool is_for_in;
} ZithForPayload;

typedef struct {
    const char *path;
    size_t path_len;
    ZithVisibility vis;
    const char *alias;
    size_t alias_len;
    bool is_export;
    bool is_from;
    int recurse_depth;   // 0 = flat, -1 = infinite, N = N levels
} ZithImportPayload;

typedef struct {
    const char **names;
    size_t *name_lens;
    size_t count;
    ZithNode *type_node;
    ZithNode *initializer;
    ZithBindingKind binding;
} ZithDestructurePayload;

typedef struct {
    const char *name;
    size_t name_len;
    ZithNode **params;
    size_t param_count;
    ZithNode *body;
} ZithMarkerPayload;

typedef struct {
    const char *name;       // field tag (NULL for untagged)
    size_t name_len;        // 0 for untagged
    ZithNode *value;
} ZithStructLitFieldPayload;

typedef struct {
    ZithNode *type_spec;    // struct type node (NULL for type inference)
    ZithNode **field_inits; // array of ZITH_NODE_STRUCT_LIT_FIELD
    size_t field_count;
} ZithStructLitPayload;

typedef struct {
    ZithLiteralKind kind;
    union {
        int64_t i64;
        uint64_t u64;
        double f64;
        struct {
            const char *ptr;
            size_t len;
        } string;
        bool boolean;
    } value;
} ZithLiteral;

ZithNode *zith_ast_make_program(ZithArena *a, ZithNode **decls, size_t count);
ZithNode *zith_ast_make_import_decl(ZithArena *arena, ZithSourceLoc loc, const ZithImportPayload *decl);

#ifdef __cplusplus
ZithNode *zith_ast_make_literal(ZithArena *a, ZithSourceLoc loc, const ZithLiteral &lit);
ZithNode *zith_ast_make_var_decl(ZithArena *a, ZithSourceLoc loc, const ZithVarPayload &decl);
ZithNode *zith_ast_make_func_decl(ZithArena *a, ZithSourceLoc loc, const ZithFuncPayload &decl);
ZithNode *zith_ast_make_union(ZithArena *a, ZithSourceLoc loc, const ZithUnionPayload &decl);
ZithNode *zith_ast_make_enum_variant(ZithArena *a, ZithSourceLoc loc, const ZithEnumVariantPayload &data);
ZithNode *zith_ast_make_array_type(ZithArena *a, ZithSourceLoc loc, ZithNode *element_type, size_t size);
ZithNode *zith_ast_make_slice_type(ZithArena *a, ZithSourceLoc loc, ZithNode *element_type);
ZithNode *zith_ast_make_array_lit(ZithArena *a, ZithSourceLoc loc, ZithNode **items, size_t count);
ZithNode *zith_ast_make_struct_lit(ZithArena *a, ZithSourceLoc loc, const ZithStructLitPayload &data);
ZithNode *zith_ast_make_struct_lit_field(ZithArena *a, ZithSourceLoc loc, const ZithStructLitFieldPayload &data);
#else
ZithNode *zith_ast_make_literal(ZithArena *a, ZithSourceLoc loc, const ZithLiteral *lit);
ZithNode *zith_ast_make_var_decl(ZithArena *a, ZithSourceLoc loc, const ZithVarPayload *decl);
ZithNode *zith_ast_make_func_decl(ZithArena *a, ZithSourceLoc loc, const ZithFuncPayload *decl);
ZithNode *zith_ast_make_union(ZithArena *a, ZithSourceLoc loc, const ZithUnionPayload *decl);
ZithNode *zith_ast_make_enum_variant(ZithArena *a, ZithSourceLoc loc, const ZithEnumVariantPayload *data);
ZithNode *zith_ast_make_array_type(ZithArena *a, ZithSourceLoc loc, ZithNode *element_type, size_t size);
ZithNode *zith_ast_make_slice_type(ZithArena *a, ZithSourceLoc loc, ZithNode *element_type);
ZithNode *zith_ast_make_array_lit(ZithArena *a, ZithSourceLoc loc, ZithNode **items, size_t count);
ZithNode *zith_ast_make_struct_lit(ZithArena *a, ZithSourceLoc loc, const ZithStructLitPayload *data);
ZithNode *zith_ast_make_struct_lit_field(ZithArena *a, ZithSourceLoc loc, const ZithStructLitFieldPayload *data);
#endif

ZithNode *zith_ast_make_identifier(ZithArena *a, ZithSourceLoc loc, const char *name, size_t len);
ZithNode *zith_ast_make_field(ZithArena *a, ZithSourceLoc loc, ZithFieldPayload field);
ZithNode *zith_ast_make_binary_op(ZithArena *a, ZithSourceLoc loc, ZithTokenType op, ZithNode *left, ZithNode *right);
ZithNode *zith_ast_make_unary_op(ZithArena *a, ZithSourceLoc loc, ZithTokenType op, ZithNode *operand, bool is_postfix);
ZithNode *zith_ast_make_call(ZithArena *a, ZithSourceLoc loc, ZithNode *callee, ZithNode **args, size_t arg_count);
ZithNode *zith_ast_make_recurse(ZithArena *a, ZithSourceLoc loc, ZithNode *callee, ZithNode **args, size_t arg_count);
ZithNode *zith_ast_make_member(ZithArena *a, ZithSourceLoc loc, ZithNode *object, ZithNode *member);
ZithNode *zith_ast_make_arrow_call(ZithArena *a, ZithSourceLoc loc, ZithNode *receiver, ZithNode *call);
ZithNode *zith_ast_make_cast(ZithArena *a, ZithSourceLoc loc, ZithNode *expr, ZithNode *type_node);
ZithNode *zith_ast_make_call_arg(ZithArena *a, ZithSourceLoc loc, ZithCallArgPayload payload);
ZithNode *zith_ast_make_param(ZithArena *a, ZithSourceLoc loc, ZithParamPayload param);
ZithNode *zith_ast_make_destructure(ZithArena *a, ZithSourceLoc loc, const ZithDestructurePayload *decl);
ZithNode *zith_ast_make_block(ZithArena *a, ZithSourceLoc loc, ZithNode **stmts, size_t count);
ZithNode *zith_ast_make_unbody(ZithArena *a, ZithSourceLoc loc, const ZithToken *tokens, size_t token_count);
ZithNode *zith_ast_make_if(ZithArena *a, ZithSourceLoc loc, ZithNode *cond, ZithNode *then_br, ZithNode *else_br);
ZithNode *zith_ast_make_for(ZithArena *a, ZithSourceLoc loc, ZithForPayload data);
ZithNode *zith_ast_make_return(ZithArena *a, ZithSourceLoc loc, ZithNode *value);
ZithNode *zith_ast_make_yield(ZithArena *a, ZithSourceLoc loc, ZithNode *value);
ZithNode *zith_ast_make_await(ZithArena *a, ZithSourceLoc loc, ZithNode *expr);
ZithNode *zith_ast_make_struct(ZithArena *a, ZithSourceLoc loc, ZithStructPayload decl);
ZithNode *zith_ast_make_enum(ZithArena *a, ZithSourceLoc loc, ZithEnumPayload decl);
ZithNode *zith_ast_make_switch(ZithArena *a, ZithSourceLoc loc, ZithSwitchPayload data);
ZithNode *zith_ast_make_try_catch(ZithArena *a, ZithSourceLoc loc, ZithTryCatchPayload data);
ZithNode *zith_ast_make_import(ZithArena *a, ZithSourceLoc loc, const ZithImportPayload &data);
ZithNode *zith_ast_make_goto(ZithArena *a, ZithSourceLoc loc, ZithGotoPayload data);
ZithNode *zith_ast_make_marker(ZithArena *a, ZithSourceLoc loc, ZithMarkerPayload data);
ZithNode *zith_ast_make_entry(ZithArena *a, ZithSourceLoc loc, ZithMarkerPayload data);
ZithNode *zith_ast_make_break(ZithArena *a, ZithSourceLoc loc, const char *label, size_t len);
ZithNode *zith_ast_make_continue(ZithArena *a, ZithSourceLoc loc, const char *label, size_t len);
ZithNode *zith_ast_make_spawn(ZithArena *a, ZithSourceLoc loc, ZithNode *body, bool is_block);
ZithNode *zith_ast_make_error(ZithArena *a, ZithSourceLoc loc, const char *msg);

typedef bool (*ZithASTVisitorFn)(ZithNode *node, void *userdata);
void zith_ast_walk(ZithNode *root, ZithASTVisitorFn pre, ZithASTVisitorFn post, void *userdata);

void zith_ast_print(const ZithNode *node, int indent);
const char *zith_ast_node_name(uint16_t id);
const char *zith_ast_fn_kind_name(ZithFnKind kind);
const char *zith_ast_literal_kind_name(ZithLiteralKind kind);
const char *zith_ast_visibility_name(ZithVisibility vis);

#ifdef __cplusplus
}

using NodeTypes = ZithNodeExtendedId;
#endif
