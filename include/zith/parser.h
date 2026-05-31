// include/zith/parser.h — Parser interface for Zith (pure C ABI)
#pragma once

#include "zith/ast.h"
#include "zith/diagnostics.h"
#include "zith/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZithSymbolTable ZithSymbolTable;

typedef struct ZithScope {
    ZithSymbolTable *parent;
    ZithSymbolTable *symbols;
} ZithScope;

typedef struct Parser {
    ZithArena *arena;
    const ZithToken *tokens;
    size_t count;
    size_t pos;

    const char *source;
    size_t source_len;
    const char *filename;
    ZithSymbolTable *currentScope;

    ZithDiagList diags;
    bool had_error;
    bool panic;

    void *diag_manager;

    ZithFnKind fn_kind;
    bool inside_fn;

    ZithVisibility current_visibility;
    int32_t current_vis_depth;
    ZithParserMode mode;
    ZithNode *scan_root;

    const char **import_roots;
    size_t import_root_count;
    bool allow_dot_imports;
} Parser;

void parser_init(Parser *p, ZithArena *arena,
                 const char *source, size_t source_len,
                 const char *filename,
                 ZithTokenStream tokens);

void parser_destroy(Parser *p);

void parser_set_import_roots(Parser *p, const char **roots, size_t count);
void parser_set_allow_dot_imports(Parser *p, bool allow);
void parser_set_imported_decls(void *decls, ZithArena *arena);

const ZithToken *parser_peek(const Parser *p);
const ZithToken *parser_peek_ahead(const Parser *p, size_t offset);
const ZithToken *parser_advance(Parser *p);
bool parser_check(const Parser *p, ZithTokenType type);
bool parser_match(Parser *p, ZithTokenType type);
const ZithToken *parser_expect(Parser *p, ZithTokenType type, const char *msg);
bool parser_is_at_end(const Parser *p);
bool parser_check_kw(const Parser *p, const char *kw);

void parser_emit_diag(Parser *p, ZithSourceLoc loc, ZithDiagSeverity severity, const char *msg);
void parser_synchronize(Parser *p);
void parser_error(Parser *p, ZithSourceLoc loc, const char *msg);
void parser_warning(Parser *p, ZithSourceLoc loc, const char *msg);
void parser_note(Parser *p, ZithSourceLoc loc, const char *msg);

void skip_block(Parser *p);

ZithNode *parser_parse_declaration(Parser *p);
ZithNode *parser_parse_statement(Parser *p);
ZithNode *parser_parse_expression(Parser *p);
ZithNode *parser_parse_type(Parser *p);
ZithNode *parser_parse_block(Parser *p);

void sema_run(Parser *p, ZithNode *root);

ZithNode *zith_parse_with_source(ZithArena *arena, const char *source, size_t source_len,
                                 const char *filename, ZithTokenStream tokens,
                                 const char **import_roots, size_t import_root_count);

ZithDiagList *zith_get_parse_diagnostics(void);

ZithNode *zith_parse_test(const char *source);
ZithNode *zith_parse_test_full(const char *source);
void zith_test_arena_destroy(void);

#ifdef __cplusplus
}
#endif
