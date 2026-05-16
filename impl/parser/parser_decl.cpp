// impl/parser/parser_decl.cpp — Top-level declarations, statements, blocks
//
// Refactored to use centralized modules and proper SCAN/PARSE separation.
// In SCAN mode, function bodies and blocks are captured as UNBODY nodes
// without parsing their contents — the parser does NOT analyze block content.
#include "zith/parser.h"
#include "memory/utils.hpp"
#include <cstring>
#include <string>
#include <vector>

using zith::ArenaList;

// Forward declarations — parser utils
extern const ZithToken *parser_peek(const Parser *p);
extern const ZithToken *parser_peek_ahead(const Parser *p, size_t offset);
extern const ZithToken *parser_advance(Parser *p);
extern bool parser_check(const Parser *p, ZithTokenType type);
extern bool parser_match(Parser *p, ZithTokenType type);
extern const ZithToken *parser_expect(Parser *p, ZithTokenType type, const char *msg);
extern void parser_error(Parser *p, ZithSourceLoc loc, const char *msg);
extern void parser_synchronize(Parser *p);
extern bool parser_check_kw(const Parser *p, const char *kw);
extern bool check_kw(const Parser *p, const char *kw);

extern ZithNode *parser_parse_type(Parser *p);
extern ZithNode *parser_parse_expression(Parser *p);
static ZithNode *parse_destructure_decl(Parser *p, ZithBindingKind binding, ZithVisibility vis, int32_t vis_depth);

// ============================================================================
// Helpers
// ============================================================================

// Captures tokens between { and } to create an UNBODY node
static ZithNode *capture_unbody(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;

    if (!parser_match(p, ZITH_TOKEN_LBRACE)) {
        // If no '{', returns nullptr (not a block)
        return nullptr;
    }

    // Marks the start of body tokens (first token after '{')
    const size_t start_pos = p->pos;
    int depth              = 1;

    // Advances until matching '}' found
    while (!parser_is_at_end(p) && depth > 0) {
        const ZithToken *t = parser_advance(p);
        if (t->type == ZITH_TOKEN_LBRACE)
            depth++;
        else if (t->type == ZITH_TOKEN_RBRACE)
            depth--;
    }

    // Calculates how many tokens are in the body (excluding '{' and '}')
    // start_pos points to the first token after '{'
    // p->pos now points to the token after '}'
    const size_t token_count = p->pos - start_pos - 1; // -1 para excluir o '}'

    // Body tokens start at start_pos
    const ZithToken *body_tokens = &p->tokens[start_pos];

    return zith_ast_make_unbody(p->arena, loc, body_tokens, token_count);
}

// ============================================================================
// Symbol Registration in SCAN mode
// ============================================================================

struct ScannedSymbolEntry {
    const char *name;
    size_t name_len;
    int kind;
    ZithVisibility visibility;
};

class ScanSymbolCollector {
public:
    static ScanSymbolCollector &instance() {
        static ScanSymbolCollector inst;
        return inst;
    }

    void clear() {
        symbols_.clear();
    }

    void add_function(const char *name, size_t len, ZithVisibility vis) {
        ScannedSymbolEntry entry{name, len, 0, vis};
        symbols_.push_back(entry);
    }

    void add_struct(const char *name, size_t len, ZithVisibility vis) {
        ScannedSymbolEntry entry{name, len, 1, vis};
        symbols_.push_back(entry);
    }

    void add_trait(const char *name, size_t len, ZithVisibility vis) {
        ScannedSymbolEntry entry{name, len, 2, vis};
        symbols_.push_back(entry);
    }

    void add_enum(const char *name, size_t len, ZithVisibility vis) {
        ScannedSymbolEntry entry{name, len, 3, vis};
        symbols_.push_back(entry);
    }

    void add_import(const char *name, size_t len, ZithVisibility vis) {
        ScannedSymbolEntry entry{name, len, 4, vis};
        symbols_.push_back(entry);
    }

    void print_symbols() const {
        size_t n = symbols_.size();
        if (n == 0) {
            printf("  (no symbols)\n");
            return;
        }
        for (size_t i = 0; i < n; ++i) {
            const auto &sym      = symbols_[i];
            const char *kind_str = "???";
            switch (sym.kind) {
            case 0:
                kind_str = "fn";
                break;
            case 1:
                kind_str = "struct";
                break;
            case 2:
                kind_str = "trait";
                break;
            case 3:
                kind_str = "enum";
                break;
            case 4:
                kind_str = "import";
                break;
            }
            const char *vis_str;
            if (sym.visibility == ZITH_VIS_PUBLIC)
                vis_str = "pub";
            else if (sym.visibility == ZITH_VIS_MODULE)
                vis_str = "mod";
            else
                vis_str = "priv";
            printf("  [%s] %.*s (%s)\n", kind_str, (int)sym.name_len, sym.name, vis_str);
        }
    }

    size_t count() const {
        return symbols_.size();
    }
    const ScannedSymbolEntry *data() const {
        return symbols_.data();
    }

private:
    ScanSymbolCollector() = default;
    std::vector<ScannedSymbolEntry> symbols_;
};

void clear_scanned_symbols() {
    ScanSymbolCollector::instance().clear();
}

static void register_fn_symbol(Parser *p, const ZithToken *name_tok, ZithVisibility vis) {
    if (name_tok && p->mode == ZITH_MODE_SCAN) {
        ScanSymbolCollector::instance().add_function(name_tok->lexeme.data, name_tok->lexeme.len,
                                                     vis);
    }
}

static void register_struct_symbol(Parser *p, const ZithToken *name_tok, ZithVisibility vis) {
    if (name_tok && p->mode == ZITH_MODE_SCAN) {
        ScanSymbolCollector::instance().add_struct(name_tok->lexeme.data, name_tok->lexeme.len,
                                                   vis);
    }
}

static void register_import_symbol(Parser *p, const char *name, size_t len, ZithVisibility vis) {
    if (name && p->mode == ZITH_MODE_SCAN) {
        ScanSymbolCollector::instance().add_import(zith_arena_str(p->arena, name, len), len, vis);
    }
}

static ZithVisibility parse_visibility(Parser *p, ZithVisibility *current_vis,
                                       int32_t *depth_out) {
    ZithVisibility vis = *current_vis;
    int32_t depth      = depth_out ? *depth_out : 0;
    bool consumed      = false;

    if (parser_check(p, ZITH_TOKEN_MODIFIER)) {
        const char *d = parser_peek(p)->lexeme.data;
        size_t l      = parser_peek(p)->lexeme.len;

        if (l == 3 && strncmp(d, "pub", 3) == 0) {
            vis   = ZITH_VIS_PUBLIC;
            depth = 0;
        } else if (l == 7 && strncmp(d, "private", 7) == 0) {
            vis   = ZITH_VIS_PRIVATE;
            depth = 0;
        } else if (l == 3 && strncmp(d, "mod", 3) == 0) {
            vis   = ZITH_VIS_MODULE;
            depth = 1;
            if (parser_peek_ahead(p, 1)->type == ZITH_TOKEN_LPAREN) {
                parser_advance(p);
                consumed = true;
                parser_advance(p);
                if (parser_peek(p)->type == ZITH_TOKEN_DOTS) {
                    depth = -1;
                    parser_advance(p);
                } else if (parser_peek(p)->type == ZITH_TOKEN_DOT &&
                           parser_peek_ahead(p, 1)->type == ZITH_TOKEN_DOT) {
                    depth = -1;
                    parser_advance(p);
                    parser_advance(p);
                } else if (parser_peek(p)->type == ZITH_TOKEN_NUMBER) {
                    const char *num     = parser_peek(p)->lexeme.data;
                    size_t num_len      = parser_peek(p)->lexeme.len;
                    depth = 0;
                    for (size_t i = 0; i < num_len; ++i)
                        if (num[i] >= '0' && num[i] <= '9')
                            depth = depth * 10 + (num[i] - '0');
                    parser_advance(p);
                }
                parser_expect(p, ZITH_TOKEN_RPAREN, "expected ')' after mod depth");
            }
        }

        if (parser_peek_ahead(p, 1)->type == ZITH_TOKEN_COLON) {
            if (!consumed)
                parser_advance(p);
            parser_advance(p);
            *current_vis = vis;
            if (depth_out)
                *depth_out = depth;
            return (ZithVisibility)-1;
        }
        if (!consumed)
            parser_advance(p);
    }
    if (depth_out)
        *depth_out = depth;
    return vis;
}

// ============================================================================
// Params & Vars
// ============================================================================

static ZithNode *parse_param(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    bool is_mutable         = false;

    // TODO: Support destructure params (ZITH_NODE_DESTRUCTURE) in a future iteration
    const ZithToken *name = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected param name");
    ZithNode *type_node   = nullptr;
    if (parser_match(p, ZITH_TOKEN_COLON)) {
        type_node = parser_parse_type(p);
    }

    ZithNode *def_val =
        parser_match(p, ZITH_TOKEN_ASSIGNMENT) ? parser_parse_expression(p) : nullptr;

    ZithOwnership own = ZITH_OWN_DEFAULT;
    if (type_node) {
        switch (type_node->type) {
        case ZITH_NODE_TYPE_UNIQUE: own = ZITH_OWN_UNIQUE; break;
        case ZITH_NODE_TYPE_SHARED: own = ZITH_OWN_SHARED; break;
        case ZITH_NODE_TYPE_VIEW:   own = ZITH_OWN_VIEW; break;
        case ZITH_NODE_TYPE_LEND:   own = ZITH_OWN_LEND; is_mutable = true; break;
        case ZITH_NODE_TYPE_PACK:   own = ZITH_OWN_PACK; break;
        case ZITH_NODE_TYPE_EXTENSION: own = ZITH_OWN_DEFAULT; break;
        }
    }

    return zith_ast_make_param(
        p->arena, loc, {name->lexeme.data, name->lexeme.len, own, type_node, def_val, is_mutable});
}

static ZithNode *parse_var_decl(Parser *p, ZithBindingKind binding, ZithVisibility vis = ZITH_VIS_PRIVATE, int32_t vis_depth = 0) {
    const ZithSourceLoc loc = parser_peek(p)->loc;

    // Check for destructure pattern: let [x, y, z]
    if (parser_check(p, ZITH_TOKEN_LBRACKET)) {
        return parse_destructure_decl(p, binding, vis, vis_depth);
    }

    const ZithToken *name = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected variable name");
    ZithNode *type_node   = parser_match(p, ZITH_TOKEN_COLON) ? parser_parse_type(p) : nullptr;

    ZithNode *init = nullptr;
    if (parser_match(p, ZITH_TOKEN_ASSIGNMENT) || parser_match(p, ZITH_TOKEN_DECLARATION)) {
        if (p->mode == ZITH_MODE_SCAN) {
            // SCAN mode: skip the expression to avoid parsing dependencies
            while (!parser_check(p, ZITH_TOKEN_SEMICOLON) && !parser_check(p, ZITH_TOKEN_COMMA) &&
                   !parser_is_at_end(p))
                parser_advance(p);
        } else {
            init = parser_parse_expression(p);
        }
    }
    parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
    return zith_ast_make_var_decl(
        p->arena, loc,
        {name->lexeme.data, name->lexeme.len, binding, ZITH_OWN_DEFAULT, vis, vis_depth, type_node, init});
}

static ZithNode *parse_destructure_decl(Parser *p, ZithBindingKind binding, ZithVisibility /*vis*/, int32_t /*vis_depth*/) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p); // consume '['

    ArenaList<const char *> names_b;
    ArenaList<size_t> name_lens_b;
    names_b.init(p->arena, 8);
    name_lens_b.init(p->arena, 8);

    while (!parser_check(p, ZITH_TOKEN_RBRACKET) && !parser_is_at_end(p)) {
        const ZithToken *name = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected variable name in destructure");
        names_b.push(p->arena, name->lexeme.data);
        name_lens_b.push(p->arena, name->lexeme.len);
        if (!parser_match(p, ZITH_TOKEN_COMMA))
            break;
    }
    parser_expect(p, ZITH_TOKEN_RBRACKET, "expected ']' closing destructure");

    // Parse optional type annotation: : type
    ZithNode *type_node = parser_match(p, ZITH_TOKEN_COLON) ? parser_parse_type(p) : nullptr;

    // Parse optional initializer: = expr  or  := expr
    ZithNode *init = nullptr;
    if (parser_match(p, ZITH_TOKEN_ASSIGNMENT) || parser_match(p, ZITH_TOKEN_DECLARATION)) {
        if (p->mode == ZITH_MODE_SCAN) {
            while (!parser_check(p, ZITH_TOKEN_SEMICOLON) && !parser_is_at_end(p))
                parser_advance(p);
        } else {
            init = parser_parse_expression(p);
        }
    }
    parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");

    size_t count = names_b.size();
    const char **names = names_b.flatten(p->arena, &count);
    size_t *name_lens = name_lens_b.flatten(p->arena, &count);

    ZithDestructurePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.names       = names;
    payload.name_lens   = name_lens;
    payload.count       = count;
    payload.type_node   = type_node;
    payload.initializer = init;
    payload.binding     = binding;

    return zith_ast_make_destructure(p->arena, loc, &payload);
}

// ============================================================================
// Bodies & Statements
// ============================================================================

static ZithNode *parse_body(Parser *p);

ZithNode *parser_parse_statement(Parser *p) {
    if (p->panic) {
        parser_synchronize(p);
        p->panic = false;
        if (parser_is_at_end(p))
            return nullptr;
    }

    /*ZithVisibility vis;
    if (check_kw(p, "public")) vis = ZITH_VIS_PUBLIC; else if (check_kw(p, "protected")) vis =
    ZITH_VIS_PROTECTED; if (parser_peek(p)->type == ZITH_TOKEN_MODIFIER) parser_advance(p);*/

    const ZithSourceLoc loc = parser_peek(p)->loc;
    switch (parser_peek(p)->type) {
    case ZITH_TOKEN_LET:
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_LET);
    case ZITH_TOKEN_VAR:
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_VAR);
    case ZITH_TOKEN_CONST:
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_CONST);
    case ZITH_TOKEN_COMPTIME:
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_COMPTIME);
    case ZITH_TOKEN_GLOBAL:
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_GLOBAL);
    case ZITH_TOKEN_IF: {
        parser_advance(p);
        ZithNode *cond   = parser_parse_expression(p);
        ZithNode *then_b = parse_body(p);
        ZithNode *else_b =
            parser_match(p, ZITH_TOKEN_ELSE)
                ? (parser_check(p, ZITH_TOKEN_IF) ? parser_parse_statement(p) : parse_body(p))
                : nullptr;
        return zith_ast_make_if(p->arena, loc, cond, then_b, else_b);
    }
    case ZITH_TOKEN_FOR: {
        parser_advance(p);
        ZithForPayload data = {};
        if (parser_check(p, ZITH_TOKEN_IDENTIFIER) &&
            parser_peek_ahead(p, 1)->type == ZITH_TOKEN_IN) {
            data.is_for_in    = true;
            data.iterator_var = parser_parse_expression(p);
            parser_advance(p); // 'in'
            data.iterable = parser_parse_expression(p);
            data.body     = parse_body(p);
        } else {
            data.condition = parser_parse_expression(p);
            data.body      = parse_body(p);
        }
        return zith_ast_make_for(p->arena, loc, data);
    }
    case ZITH_TOKEN_RETURN: {
        parser_advance(p);
        ZithNode *val = (!parser_check(p, ZITH_TOKEN_SEMICOLON) && !parser_is_at_end(p))
                            ? parser_parse_expression(p)
                            : nullptr;
        parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
        return zith_ast_make_return(p->arena, loc, val);
    }
    // Nested functions handled by parser_parse_declaration usually, but here for stmt-position
    case ZITH_TOKEN_FN:
    case ZITH_TOKEN_ASYNC:
        return parser_parse_declaration(p);
    case ZITH_TOKEN_LBRACE:
        return parser_parse_block(p);
    default: {
        ZithNode *expr = parser_parse_expression(p);
        if (parser_match(p, ZITH_TOKEN_ASSIGNMENT) || parser_match(p, ZITH_TOKEN_PLUS_EQUAL) ||
            parser_match(p, ZITH_TOKEN_MINUS_EQUAL)) {
            const ZithSourceLoc aloc = parser_peek(p)->loc;
            ZithToken op_t           = *parser_peek(p);
            parser_advance(p);
            expr = zith_ast_make_binary_op(p->arena, aloc, op_t.type, expr,
                                           parser_parse_expression(p));
        }
        parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
        return expr;
    }
    }
}

ZithNode *parser_parse_block(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    parser_expect(p, ZITH_TOKEN_LBRACE, "expected '{'");
    ArenaList<ZithNode *> stmts_b;
    stmts_b.init(p->arena, 16);
    while (!parser_check(p, ZITH_TOKEN_RBRACE) && !parser_is_at_end(p))
        stmts_b.push(p->arena, parser_parse_statement(p));
    parser_expect(p, ZITH_TOKEN_RBRACE, "expected '}'");
    size_t count     = 0;
    ZithNode **stmts = stmts_b.flatten(p->arena, &count);
    return zith_ast_make_block(p->arena, loc, stmts, count);
}

static ZithNode *parse_body(Parser *p) {
    if (parser_match(p, ZITH_TOKEN_LBRACE)) {
        ArenaList<ZithNode *> stmts_b;
        stmts_b.init(p->arena, 16);
        while (!parser_check(p, ZITH_TOKEN_RBRACE) && !parser_is_at_end(p))
            stmts_b.push(p->arena, parser_parse_statement(p));
        parser_expect(p, ZITH_TOKEN_RBRACE, "expected '}'");
        size_t count     = 0;
        ZithNode **stmts = stmts_b.flatten(p->arena, &count);
        return zith_ast_make_block(p->arena, parser_peek(p)->loc, stmts, count);
    }
    ZithNode *stmt = parser_parse_statement(p);
    if (!stmt)
        return zith_ast_make_block(p->arena, parser_peek(p)->loc, nullptr, 0);
    ZithNode **arr = (ZithNode **)zith_arena_alloc(p->arena, sizeof(ZithNode *));
    if (arr)
        *arr = stmt;
    return zith_ast_make_block(p->arena, parser_peek(p)->loc, arr, arr ? 1 : 0);
}

// ============================================================================
// Top-Level Declarations
// ============================================================================

static ZithNode *parse_fn_decl(Parser *p, ZithSourceLoc loc, ZithVisibility vis, int32_t vis_depth, bool is_method) {
    ZithFnKind kind = ZITH_FN_NORMAL;
    bool fn_consumed = false;
    if (parser_match(p, ZITH_TOKEN_ASYNC)) {
        kind = ZITH_FN_ASYNC;
    } else if (parser_match(p, ZITH_TOKEN_RAW)) {
        kind = ZITH_FN_RAW;
        fn_consumed = true;
    } else if (parser_match(p, ZITH_TOKEN_CONST)) {
        kind = ZITH_FN_CONST;
        fn_consumed = true;
    } else if (parser_match(p, ZITH_TOKEN_COMPTIME)) {
        kind = ZITH_FN_COMPTIME;
        fn_consumed = true;
    } else if (parser_match(p, ZITH_TOKEN_FLOWING)) {
        kind = ZITH_FN_FLOWING;
    } else if (parser_match(p, ZITH_TOKEN_NORETURN)) {
        kind = ZITH_FN_NORETURN;
    }

    if (!fn_consumed)
        parser_expect(p, ZITH_TOKEN_FN, "expected 'fn' keyword");
    else
        parser_expect(p, ZITH_TOKEN_FN, "expected 'fn' keyword after fn-kind modifier");
    const ZithToken *name = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected function name");

    // Register function symbol in SCAN mode
    register_fn_symbol(p, name, vis);

    parser_expect(p, ZITH_TOKEN_LPAREN, "expected '('");
    ArenaList<ZithNode *> params_b;
    params_b.init(p->arena, 8);

    while (!parser_check(p, ZITH_TOKEN_RPAREN) && !parser_is_at_end(p)) {
        params_b.push(p->arena, parse_param(p));
        if (!parser_match(p, ZITH_TOKEN_COMMA))
            break;
    }
    parser_expect(p, ZITH_TOKEN_RPAREN, "expected ')'");

    ZithNode *ret_type = nullptr;
    if (kind != ZITH_FN_NORETURN &&
        (parser_match(p, ZITH_TOKEN_ARROW) || parser_match(p, ZITH_TOKEN_COLON)))
        ret_type = parser_parse_type(p);

    ZithNode *body = nullptr;
    if (parser_match(p, ZITH_TOKEN_SEMICOLON)) {
        // Prototype/forward declaration — no body
    } else if (p->mode == ZITH_MODE_SCAN) {
        // SCAN mode: captures the body as UNBODY instead of skipping it entirely
        if (!parser_match(p, ZITH_TOKEN_COLON)) {
            body = capture_unbody(p);
        }
    } else {
        // PARSE mode: parses the body normally
        if (!parser_check(p, ZITH_TOKEN_COLON))
            body = parse_body(p);
        else
            parser_advance(p);
    }

    size_t pcount     = 0;
    ZithNode **params = params_b.flatten(p->arena, &pcount);
    return zith_ast_make_func_decl(p->arena, loc,
                                   {name->lexeme.data, name->lexeme.len, kind, params, pcount,
                                    ret_type, body, vis, vis_depth, is_method});
}

static ZithNode *parse_struct_decl(Parser *p, ZithVisibility struct_vis, int32_t struct_depth) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p); // consume 'struct'
    const ZithToken *name = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected struct name");

    // Register struct symbol in SCAN mode
    register_struct_symbol(p, name, struct_vis);

    parser_expect(p, ZITH_TOKEN_LBRACE, "expected '{'");

    ArenaList<ZithNode *> fields_b, methods_b;
    fields_b.init(p->arena, 8);
    methods_b.init(p->arena, 4);
    ZithVisibility current_vis = struct_vis;

    while (!parser_check(p, ZITH_TOKEN_RBRACE) && !parser_is_at_end(p)) {
        // 1. Parse Visibility (public/private/protected)
        int32_t item_depth = 0;
        ZithVisibility item_vis = parse_visibility(p, &current_vis, &item_depth);
        if ((int)item_vis == -1)
            continue;

        // 2. Check for Methods (fn, async, raw fn, const fn, etc)
        if (parser_check(p, ZITH_TOKEN_FN) || parser_check(p, ZITH_TOKEN_ASYNC) ||
            parser_check(p, ZITH_TOKEN_FLOWING) || parser_check(p, ZITH_TOKEN_NORETURN) ||
            parser_check(p, ZITH_TOKEN_RAW) ||
            (parser_check(p, ZITH_TOKEN_CONST) && parser_peek_ahead(p, 1)->type == ZITH_TOKEN_FN)) {
            methods_b.push(p->arena, parse_fn_decl(p, parser_peek(p)->loc, item_vis, item_depth, true));
            continue;
        }

        // 3. Check for Fields
        // Matches: x: i32,  or  [x,y]: i32,  or  [x,y]: |i32,u64|,
        //          let x: i32,  global x: i32,  const x: i32,  comptime x: i32,
        if (parser_check(p, ZITH_TOKEN_LBRACKET) ||
            parser_check(p, ZITH_TOKEN_LET) || parser_check(p, ZITH_TOKEN_VAR) ||
            parser_check(p, ZITH_TOKEN_GLOBAL) || parser_check(p, ZITH_TOKEN_CONST) ||
            parser_check(p, ZITH_TOKEN_COMPTIME) ||
            parser_check(p, ZITH_TOKEN_IDENTIFIER)) {

            // Consume binding keyword if present (optional)
            if (parser_match(p, ZITH_TOKEN_LET) || parser_match(p, ZITH_TOKEN_VAR) ||
                parser_match(p, ZITH_TOKEN_GLOBAL) || parser_match(p, ZITH_TOKEN_CONST) ||
                parser_match(p, ZITH_TOKEN_COMPTIME)) {
            }

            // Check for destructure syntax: [name1, name2, ...]: type
            if (parser_match(p, ZITH_TOKEN_LBRACKET)) {
                const ZithSourceLoc floc = parser_peek(p)->loc;

                ArenaList<const char *> fnames_b;
                ArenaList<size_t> fname_lens_b;
                fnames_b.init(p->arena, 8);
                fname_lens_b.init(p->arena, 8);
                while (!parser_check(p, ZITH_TOKEN_RBRACKET) && !parser_is_at_end(p)) {
                    const ZithToken *tok = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected field name");
                    fnames_b.push(p->arena, tok->lexeme.data);
                    fname_lens_b.push(p->arena, tok->lexeme.len);
                    if (!parser_match(p, ZITH_TOKEN_COMMA))
                        break;
                }
                parser_expect(p, ZITH_TOKEN_RBRACKET, "expected ']' closing destructure");

                ZithNode *ftype = nullptr;
                if (parser_match(p, ZITH_TOKEN_COLON)) {
                    ftype = parser_parse_type(p);
                }

                ZithNode *fdef = nullptr;
                if (parser_match(p, ZITH_TOKEN_ASSIGNMENT)) {
                    if (p->mode == ZITH_MODE_SCAN) {
                        while (!parser_check(p, ZITH_TOKEN_COMMA) &&
                               !parser_check(p, ZITH_TOKEN_RBRACE) && !parser_is_at_end(p)) {
                            parser_advance(p);
                        }
                    } else {
                        fdef = parser_parse_expression(p);
                    }
                }

                if (parser_peek(p)->type != ZITH_TOKEN_RBRACE)
                    parser_expect(p, ZITH_TOKEN_COMMA, "expected ','");

                size_t name_count = fnames_b.size();
                const char **names = fnames_b.flatten(p->arena, &name_count);
                size_t *name_lens = fname_lens_b.flatten(p->arena, &name_count);

                if (ftype && ftype->type == ZITH_NODE_TYPE_TUPLE) {
                    ZithNode **type_items = static_cast<ZithNode **>(ftype->data.list.ptr);
                    size_t type_count = ftype->data.list.len;
                    if (name_count != type_count) {
                        char buf[256];
                        snprintf(buf, sizeof(buf),
                                 "destructure field count mismatch: %zu names but %zu types",
                                 name_count, type_count);
                        parser_emit_diag(p, floc, ZITH_DIAG_ERROR, buf);
                    }
                    for (size_t i = 0; i < name_count; ++i) {
                        ZithNode *elem_type = (i < type_count) ? type_items[i] : nullptr;
                        ZithOwnership elem_own = ZITH_OWN_DEFAULT;
                        if (elem_type) {
                            switch (elem_type->type) {
                            case ZITH_NODE_TYPE_UNIQUE: elem_own = ZITH_OWN_UNIQUE; break;
                            case ZITH_NODE_TYPE_SHARED: elem_own = ZITH_OWN_SHARED; break;
                            case ZITH_NODE_TYPE_VIEW:   elem_own = ZITH_OWN_VIEW; break;
                            case ZITH_NODE_TYPE_LEND:   elem_own = ZITH_OWN_LEND; break;
                            case ZITH_NODE_TYPE_PACK:   elem_own = ZITH_OWN_PACK; break;
                            case ZITH_NODE_TYPE_EXTENSION: elem_own = ZITH_OWN_DEFAULT; break;
                            }
                        }
                        fields_b.push(p->arena, zith_ast_make_field(p->arena, floc,
                            {names[i], name_lens[i], elem_own, item_vis, item_depth, elem_type, fdef}));
                    }
                } else {
                    ZithOwnership field_own = ZITH_OWN_DEFAULT;
                    if (ftype) {
                        switch (ftype->type) {
                        case ZITH_NODE_TYPE_UNIQUE: field_own = ZITH_OWN_UNIQUE; break;
                        case ZITH_NODE_TYPE_SHARED: field_own = ZITH_OWN_SHARED; break;
                        case ZITH_NODE_TYPE_VIEW:   field_own = ZITH_OWN_VIEW; break;
                        case ZITH_NODE_TYPE_LEND:   field_own = ZITH_OWN_LEND; break;
                        case ZITH_NODE_TYPE_PACK:   field_own = ZITH_OWN_PACK; break;
                        case ZITH_NODE_TYPE_EXTENSION: field_own = ZITH_OWN_DEFAULT; break;
                        }
                    }
                    for (size_t i = 0; i < name_count; ++i) {
                        fields_b.push(p->arena, zith_ast_make_field(p->arena, floc,
                            {names[i], name_lens[i], field_own, item_vis, item_depth, ftype, fdef}));
                    }
                }
                continue;
            }

            const ZithSourceLoc floc = parser_peek(p)->loc;

            // Parse Name
            const ZithToken *fname = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected field name");

            // Parse Type (Optional: x: i32)
            ZithNode *ftype = nullptr;
            if (parser_match(p, ZITH_TOKEN_COLON)) {
                ftype = parser_parse_type(p);
            }

            // Parse Default Value (Optional: x: i32 = 10)
            ZithNode *fdef = nullptr;
            if (parser_match(p, ZITH_TOKEN_ASSIGNMENT)) {
                if (p->mode == ZITH_MODE_SCAN) {
                    // In scan mode, skip the expression to avoid parsing dependencies
                    while (!parser_check(p, ZITH_TOKEN_COMMA) &&
                           !parser_check(p, ZITH_TOKEN_RBRACE) && !parser_is_at_end(p)) {
                        parser_advance(p);
                    }
                } else {
                    fdef = parser_parse_expression(p);
                }
            }

            ZithOwnership field_own = ZITH_OWN_DEFAULT;
            if (ftype) {
                switch (ftype->type) {
                case ZITH_NODE_TYPE_UNIQUE: field_own = ZITH_OWN_UNIQUE; break;
                case ZITH_NODE_TYPE_SHARED: field_own = ZITH_OWN_SHARED; break;
                case ZITH_NODE_TYPE_VIEW:   field_own = ZITH_OWN_VIEW; break;
                case ZITH_NODE_TYPE_LEND:   field_own = ZITH_OWN_LEND; break;
                case ZITH_NODE_TYPE_PACK:   field_own = ZITH_OWN_PACK; break;
                case ZITH_NODE_TYPE_EXTENSION: field_own = ZITH_OWN_DEFAULT; break;
                }
            }

            // Use comma as separator, but allow last field to omit it before '}'
            if (parser_peek(p)->type != ZITH_TOKEN_RBRACE)
                parser_expect(p, ZITH_TOKEN_COMMA, "expected ',' or ';'");

            fields_b.push(p->arena, zith_ast_make_field(p->arena, floc,
                                                         {fname->lexeme.data, fname->lexeme.len, field_own,
                                                          item_vis, item_depth, ftype, fdef}));
            continue;
        }

        // If we get here, we encountered a token we don't understand in the struct body
        parser_error(p, parser_peek(p)->loc, "unexpected token in struct body");
        parser_advance(p); // Skip it to avoid infinite loop
    }

    parser_expect(p, ZITH_TOKEN_RBRACE, "expected '}'");

    size_t fc = 0, mc = 0;
    ZithNode **fields  = fields_b.flatten(p->arena, &fc);
    ZithNode **methods = methods_b.flatten(p->arena, &mc);
    return zith_ast_make_struct(
        p->arena, loc, {name->lexeme.data, name->lexeme.len, fields, fc, methods, mc, struct_vis, struct_depth});
}

static ZithNode *parse_import_decl(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p);     // consume 'import'
    char buf[256];
    size_t buf_len = 0;

    // Primeiro segmento (espera IDENTIFIER)
    const ZithToken *seg = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected module name");
    if (seg->lexeme.len < sizeof(buf)) {
        memcpy(buf, seg->lexeme.data, seg->lexeme.len);
        buf_len = seg->lexeme.len;
    }

    // Additional segments separated by '.' or '/'
    while (true) {
        if (parser_match(p, ZITH_TOKEN_DOT)) {
            if (buf_len < sizeof(buf) - 1)
                buf[buf_len++] = '.';
        } else if (parser_match(p, ZITH_TOKEN_DIVIDE)) {
            if (buf_len < sizeof(buf) - 1)
                buf[buf_len++] = '/';
        } else {
            break;
        }
        seg = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected identifier after path separator");
        if (buf_len + seg->lexeme.len < sizeof(buf)) {
            memcpy(buf + buf_len, seg->lexeme.data, seg->lexeme.len);
            buf_len += seg->lexeme.len;
        }
    }

    // Suporte a alias: import x as y
    const char *alias = nullptr;
    size_t alias_len  = 0;
    if (parser_match(p, ZITH_TOKEN_AS)) {
        const ZithToken *alias_tok = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected alias name");
        alias                      = alias_tok->lexeme.data;
        alias_len                  = alias_tok->lexeme.len;
    }

    parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
    ZithImportPayload payload = {zith_arena_str(p->arena, buf, buf_len),
                                 buf_len,
                                 ZITH_VIS_PRIVATE,
                                 alias,
                                 alias_len,
                                 false,
                                 false};
    register_import_symbol(p, buf, buf_len, ZITH_VIS_PRIVATE);

    if (p->mode == ZITH_MODE_SCAN && p->import_roots && p->import_root_count > 0) {
        std::string import_path(buf, buf_len);

        std::string root;
        std::string rel_path;
        size_t slash_pos = import_path.find('/');
        if (slash_pos != std::string::npos) {
            root     = import_path.substr(0, slash_pos);
            rel_path = import_path.substr(slash_pos + 1);
        } else {
            size_t dot_pos = import_path.find('.');
            if (dot_pos != std::string::npos) {
                if (p->allow_dot_imports) {
                    root     = import_path.substr(0, dot_pos);
                    rel_path = import_path.substr(dot_pos + 1);
                } else {
                    parser_emit_diag(p, loc, ZITH_DIAG_ERROR,
                                     "dot-separated import paths are disabled; use '/' instead");
                    root     = import_path;
                    rel_path = "";
                }
            } else {
                root     = import_path;
                rel_path = "";
            }
        }

        bool allowed = false;
        std::string root_dir;
        for (size_t i = 0; i < p->import_root_count; ++i) {
            const char *root_full = p->import_roots[i];
            const char *slash     = strrchr(root_full, '/');
            const char *root_name = slash ? slash + 1 : root_full;
            if (root == root_name) {
                allowed = true;
                root_dir = root_full;
                break;
            }
        }

        if (allowed && !rel_path.empty()) {
            std::string rel_fs_path;
            rel_fs_path.reserve(rel_path.size());
            for (char c : rel_path) rel_fs_path.push_back(c == '.' ? '/' : c);
            std::string file_path = root_dir + "/" + rel_fs_path + ".zith";
            size_t file_size      = 0;
            char *source = zith_load_file_to_arena(p->arena, file_path.c_str(), &file_size);

            if (source && file_size > 0) {
                ZithTokenStream tokens = zith_tokenize(p->arena, source, file_size);
                if (tokens.data) {
                    Parser imp_parser;
                    parser_init(&imp_parser, p->arena, source, file_size, file_path.c_str(),
                                tokens);
                    imp_parser.mode = ZITH_MODE_SCAN;

                    ArenaList<ZithNode *> import_decls;
                    import_decls.init(p->arena, 16);
                    while (!parser_is_at_end(&imp_parser)) {
                        size_t pb   = imp_parser.pos;
                        ZithNode *d = parser_parse_declaration(&imp_parser);
                        if (d)
                            import_decls.push(p->arena, d);
                        if (imp_parser.pos == pb && !parser_is_at_end(&imp_parser))
                            parser_advance(&imp_parser);
                    }

                    if (import_decls.size() > 0) {
                        extern void parser_set_imported_decls(void *decls, ZithArena *arena);
                        parser_set_imported_decls(&import_decls, p->arena);
                    }
                }
            }
        }
    }

    return zith_ast_make_import(p->arena, loc, payload);
}

static ZithNode *parse_from_import_decl(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p); // consume 'from'

    // Parse the base module (e.g. std.io.console or std/io/console)
    char module_buf[256];
    size_t module_len    = 0;
    const ZithToken *seg = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected module name");
    if (seg->lexeme.len < sizeof(module_buf)) {
        memcpy(module_buf, seg->lexeme.data, seg->lexeme.len);
        module_len = seg->lexeme.len;
    }

    while (true) {
        if (parser_match(p, ZITH_TOKEN_DOT)) {
            if (module_len < sizeof(module_buf) - 1)
                module_buf[module_len++] = '.';
        } else if (parser_match(p, ZITH_TOKEN_DIVIDE)) {
            if (module_len < sizeof(module_buf) - 1)
                module_buf[module_len++] = '/';
        } else {
            break;
        }
        seg = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected identifier after path separator");
        if (module_len + seg->lexeme.len < sizeof(module_buf)) {
            memcpy(module_buf + module_len, seg->lexeme.data, seg->lexeme.len);
            module_len += seg->lexeme.len;
        }
    }

    parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");

    ZithImportPayload payload = {
        zith_arena_str(p->arena, module_buf, module_len),
        module_len,
        ZITH_VIS_PRIVATE,
        nullptr,
        0,
        false,
        true
    };
    register_import_symbol(p, module_buf, module_len, ZITH_VIS_PRIVATE);

    if (p->mode == ZITH_MODE_SCAN && p->import_roots && p->import_root_count > 0) {
        std::string import_path(module_buf, module_len);

        std::string root;
        std::string rel_path;
        size_t slash_pos = import_path.find('/');
        if (slash_pos != std::string::npos) {
            root     = import_path.substr(0, slash_pos);
            rel_path = import_path.substr(slash_pos + 1);
        } else {
            size_t dot_pos = import_path.find('.');
            if (dot_pos != std::string::npos) {
                if (p->allow_dot_imports) {
                    root     = import_path.substr(0, dot_pos);
                    rel_path = import_path.substr(dot_pos + 1);
                } else {
                    parser_emit_diag(p, loc, ZITH_DIAG_ERROR,
                                     "dot-separated import paths are disabled; use '/' instead");
                    root     = import_path;
                    rel_path = "";
                }
            } else {
                root     = import_path;
                rel_path = "";
            }
        }

        bool allowed = false;
        std::string root_dir;
        for (size_t i = 0; i < p->import_root_count; ++i) {
            const char *root_full = p->import_roots[i];
            const char *slash     = strrchr(root_full, '/');
            const char *root_name = slash ? slash + 1 : root_full;
            if (root == root_name) {
                allowed = true;
                root_dir = root_full;
                break;
            }
        }

        if (allowed && !rel_path.empty()) {
            std::string rel_fs_path;
            rel_fs_path.reserve(rel_path.size());
            for (char c : rel_path) rel_fs_path.push_back(c == '.' ? '/' : c);
            std::string file_path = root_dir + "/" + rel_fs_path + ".zith";
            size_t file_size      = 0;
            char *source = zith_load_file_to_arena(p->arena, file_path.c_str(), &file_size);

            if (source && file_size > 0) {
                ZithTokenStream tokens = zith_tokenize(p->arena, source, file_size);
                if (tokens.data) {
                    Parser imp_parser;
                    parser_init(&imp_parser, p->arena, source, file_size, file_path.c_str(),
                                tokens);
                    imp_parser.mode = ZITH_MODE_SCAN;

                    ArenaList<ZithNode *> import_decls;
                    import_decls.init(p->arena, 16);
                    while (!parser_is_at_end(&imp_parser)) {
                        size_t pb   = imp_parser.pos;
                        ZithNode *d = parser_parse_declaration(&imp_parser);
                        if (d)
                            import_decls.push(p->arena, d);
                        if (imp_parser.pos == pb && !parser_is_at_end(&imp_parser))
                            parser_advance(&imp_parser);
                    }

                    if (import_decls.size() > 0) {
                        extern void parser_set_imported_decls(void *decls, ZithArena *arena);
                        parser_set_imported_decls(&import_decls, p->arena);
                    }
                }
            }
        }
    }

    return zith_ast_make_import(p->arena, loc, payload);
}

static ZithNode *parse_export_decl(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    parser_advance(p); // consume 'export'
    char buf[256];
    size_t buf_len = 0;

    // First segment (expects IDENTIFIER)
    const ZithToken *seg = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected module name");
    if (seg->lexeme.len < sizeof(buf)) {
        memcpy(buf, seg->lexeme.data, seg->lexeme.len);
        buf_len = seg->lexeme.len;
    }

    // Additional segments separated by '.'
    while (parser_match(p, ZITH_TOKEN_DOT)) {
        if (buf_len < sizeof(buf) - 1)
            buf[buf_len++] = '.';
        seg = parser_expect(p, ZITH_TOKEN_IDENTIFIER, "expected identifier after '.'");
        if (buf_len + seg->lexeme.len < sizeof(buf)) {
            memcpy(buf + buf_len, seg->lexeme.data, seg->lexeme.len);
            buf_len += seg->lexeme.len;
        }
    }

    parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
    auto alias                = "";
    size_t alias_len          = 1;
    ZithImportPayload payload = {zith_arena_str(p->arena, buf, buf_len),
                                 buf_len,
                                 ZITH_VIS_PUBLIC,
                                 alias,
                                 alias_len,
                                 true,
                                 false};
    register_import_symbol(p, buf, buf_len, ZITH_VIS_PUBLIC);

    // In SCAN mode, try to load and import the module
    if (p->mode == ZITH_MODE_SCAN && p->import_roots && p->import_root_count > 0) {
        std::string import_path(buf, buf_len);

        // Convert path format: std/io/console -> root=std, path=io/console
        std::string root;
        std::string rel_path;
        size_t slash_pos = import_path.find('/');
        if (slash_pos != std::string::npos) {
            root     = import_path.substr(0, slash_pos);
            rel_path = import_path.substr(slash_pos + 1);
        } else {
            size_t dot_pos = import_path.find('.');
            if (dot_pos != std::string::npos) {
                root     = import_path.substr(0, dot_pos);
                rel_path = import_path.substr(dot_pos + 1);
            } else {
                root     = import_path;
                rel_path = "";
            }
        }

        // Check if root is allowed
        bool allowed = false;
        std::string root_dir;
        for (size_t i = 0; i < p->import_root_count; ++i) {
            const char *root_full = p->import_roots[i];
            const char *slash     = strrchr(root_full, '/');
            const char *root_name = slash ? slash + 1 : root_full;
            if (root == root_name) {
                allowed = true;
                root_dir = root_full;
                break;
            }
        }

        if (allowed && !rel_path.empty()) {
            std::string rel_fs_path;
            rel_fs_path.reserve(rel_path.size());
            for (char c : rel_path) rel_fs_path.push_back(c == '.' ? '/' : c);
            std::string file_path = root_dir + "/" + rel_fs_path + ".zith";
            size_t file_size      = 0;
            char *source = zith_load_file_to_arena(p->arena, file_path.c_str(), &file_size);

            if (source && file_size > 0) {
                ZithTokenStream tokens = zith_tokenize(p->arena, source, file_size);
                if (tokens.data) {
                    Parser imp_parser;
                    parser_init(&imp_parser, p->arena, source, file_size, file_path.c_str(),
                                tokens);
                    imp_parser.mode = ZITH_MODE_SCAN;

                    ArenaList<ZithNode *> import_decls;
                    import_decls.init(p->arena, 16);
                    while (!parser_is_at_end(&imp_parser)) {
                        size_t pb   = imp_parser.pos;
                        ZithNode *d = parser_parse_declaration(&imp_parser);
                        if (d)
                            import_decls.push(p->arena, d);
                        if (imp_parser.pos == pb && !parser_is_at_end(&imp_parser))
                            parser_advance(&imp_parser);
                    }

                    if (import_decls.size() > 0) {
                        extern void parser_set_imported_decls(void *decls, ZithArena *arena);
                        parser_set_imported_decls(&import_decls, p->arena);
                    }
                }
            }
        }
    }

    return zith_ast_make_import(p->arena, loc, payload);
}

ZithNode *parser_parse_declaration(Parser *p) {
    if (p->panic) {
        parser_synchronize(p);
        p->panic = false;
        if (parser_is_at_end(p))
            return nullptr;
    }

    int32_t vis_depth = 0;
    ZithVisibility vis = parse_visibility(p, &p->current_visibility, &vis_depth);
    p->current_vis_depth = vis_depth;
    if ((int)vis == -1)
        return nullptr;

    const ZithToken *t      = parser_peek(p);
    const ZithSourceLoc loc = t->loc;

    if (t->type == ZITH_TOKEN_FN || t->type == ZITH_TOKEN_ASYNC || t->type == ZITH_TOKEN_FLOWING ||
        t->type == ZITH_TOKEN_NORETURN || t->type == ZITH_TOKEN_RAW)
        return parse_fn_decl(p, loc, vis, vis_depth, false);

    if (t->type == ZITH_TOKEN_STRUCT)
        return parse_struct_decl(p, vis, vis_depth);
    if (t->type == ZITH_TOKEN_IMPORT)
        return parse_import_decl(p);
    if (t->type == ZITH_TOKEN_FROM)
        return parse_from_import_decl(p);
    if (t->type == ZITH_TOKEN_EXPORT)
        return parse_export_decl(p);
    if (t->type == ZITH_TOKEN_CONST) {
        // Distinguish const fn vs const variable declaration
        if (parser_peek_ahead(p, 1)->type == ZITH_TOKEN_FN)
            return parse_fn_decl(p, loc, vis, vis_depth, false);
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_CONST, vis, vis_depth);
    }
    if (t->type == ZITH_TOKEN_COMPTIME) {
        if (parser_peek_ahead(p, 1)->type == ZITH_TOKEN_FN)
            return parse_fn_decl(p, loc, vis, vis_depth, false);
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_COMPTIME, vis, vis_depth);
    }
    if (t->type == ZITH_TOKEN_GLOBAL) {
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_GLOBAL, vis, vis_depth);
    }

    ZithNode *expr = parser_parse_expression(p);
    parser_match(p, ZITH_TOKEN_SEMICOLON);
    return expr;
}

#ifdef __cplusplus
void print_scanned_symbols() {
    printf("Scanned symbols:\n");
    ScanSymbolCollector::instance().print_symbols();
}
#endif