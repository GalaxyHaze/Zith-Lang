// impl/parser/parser_decl.cpp — Top-level declarations, statements, blocks
//
// Refactored to use centralized modules and proper SCAN/PARSE separation.
// In SCAN mode, function bodies and blocks are captured as UNBODY nodes
// without parsing their contents — the parser does NOT analyze block content.
#include <import/import.hpp>
#include <zith/parser.h>
#include <memory/utils.hpp>
#include <import/module_registry.hpp>
#include <cstring>
#include <memory>
#include <string>
#include <filesystem>
#include <climits>

using zith::ArenaList;

// Forward declarations — parser utils









extern bool check_kw(const Parser *p, const char *kw);




extern ZithOwnership parser_ownership_from_node(const ZithNode *type_node);
extern ZithNode *parser_make_list_node(Parser *p, ZithSourceLoc loc,
                                       uint16_t type,
                                       ZithNode *(*parse_fn)(Parser *),
                                       const char *error_msg);
extern ZithNode *parser_parse_block_body(Parser *p, bool expect_brace, ZithSourceLoc loc);
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
// Symbol Registration in SCAN mode - Using ModuleRegistry
// ============================================================================

static zith::import::Visibility to_import_visibility(ZithVisibility vis) {
    return (vis == ZITH_VIS_PUBLIC)  ? zith::import::Visibility::Public :
           (vis == ZITH_VIS_MODULE) ? zith::import::Visibility::Module :
                                      zith::import::Visibility::Private;
}

static std::string parser_module_key(const Parser *p) {
    if (p->filename && p->filename[0] != '\0')
        return p->filename;
    return {};
}

static std::string parser_module_path(const Parser *p) {
    return parser_module_key(p);
}

static zith::import::Module *ensure_parser_module(Parser *p) {
    if (p->mode != ZITH_MODE_SCAN)
        return nullptr;

    auto &registry = zith::import::ModuleRegistry::instance();
    const std::string module_key  = parser_module_key(p);
    const std::string module_path = parser_module_path(p);
    if (module_key.empty())
        return nullptr;

    auto mod = registry.get_module(module_key);
    if (!mod) {
        zith::import::Module new_mod(module_key, std::filesystem::path(module_path));
        registry.register_module(std::move(new_mod));
        mod = registry.get_module(module_key);
    }
    return mod.get();
}

static void register_symbol(Parser *p, const char *name, size_t len, int32_t line,
                            ZithVisibility vis, zith::import::SymbolKind kind) {
    if (!name || len == 0)
        return;

    auto mod = ensure_parser_module(p);
    if (!mod)
        return;

    const std::string source_path = parser_module_path(p);
    zith::import::SourceLocation loc(source_path, line, 0);
    zith::import::SymbolEntry entry(std::string(name, len), kind, to_import_visibility(vis), loc);
    mod->add_symbol(std::move(entry));
}

static void register_fn_symbol(Parser *p, const ZithToken *name_tok, ZithVisibility vis) {
    if (!name_tok)
        return;
    register_symbol(p, name_tok->lexeme.data, name_tok->lexeme.len, name_tok->loc.line, vis,
                    zith::import::SymbolKind::Function);
}

static void register_struct_symbol(Parser *p, const ZithToken *name_tok, ZithVisibility vis) {
    if (!name_tok)
        return;
    register_symbol(p, name_tok->lexeme.data, name_tok->lexeme.len, name_tok->loc.line, vis,
                    zith::import::SymbolKind::Struct);
}

static void register_import_symbol(Parser *p, const char *name, size_t len, ZithVisibility vis) {
    register_symbol(p, name, len, 0, vis, zith::import::SymbolKind::Module);
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
            return static_cast<ZithVisibility>(-1);
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

    ZithOwnership own = parser_ownership_from_node(type_node);
    if (type_node && type_node->type == ZITH_NODE_TYPE_LEND)
        is_mutable = true;

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
    case ZITH_TOKEN_AUTO:
        parser_advance(p);
        return parse_var_decl(p, ZITH_BINDING_AUTO);
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
        ZithTokenType assign_type = ZITH_TOKEN_END;
        if (parser_match(p, ZITH_TOKEN_ASSIGNMENT))
            assign_type = ZITH_TOKEN_ASSIGNMENT;
        else if (parser_match(p, ZITH_TOKEN_PLUS_EQUAL))
            assign_type = ZITH_TOKEN_PLUS_EQUAL;
        else if (parser_match(p, ZITH_TOKEN_MINUS_EQUAL))
            assign_type = ZITH_TOKEN_MINUS_EQUAL;

        if (assign_type != ZITH_TOKEN_END) {
            expr = zith_ast_make_binary_op(p->arena, parser_peek(p)->loc, assign_type, expr,
                                           parser_parse_expression(p));
        }
        parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
        return expr;
    }
    }
}

ZithNode *parser_parse_block(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    return parser_parse_block_body(p, true, loc);
}

static ZithNode *parse_body(Parser *p) {
    const ZithSourceLoc loc = parser_peek(p)->loc;
    ZithNode *block = parser_parse_block_body(p, false, loc);
    if (block)
        return block;
    ZithNode *stmt = parser_parse_statement(p);
    if (!stmt)
        return zith_ast_make_block(p->arena, parser_peek(p)->loc, nullptr, 0);
    ZithNode **arr = static_cast<ZithNode **>(zith_arena_alloc(p->arena, sizeof(ZithNode *)));
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
                        ZithOwnership elem_own = parser_ownership_from_node(
                            (i < type_count) ? type_items[i] : nullptr);
                        fields_b.push(p->arena, zith_ast_make_field(p->arena, floc,
                            {names[i], name_lens[i], elem_own, item_vis, item_depth, elem_type, fdef}));
                    }
                } else {
                    ZithOwnership field_own = parser_ownership_from_node(ftype);
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

            ZithOwnership field_own = parser_ownership_from_node(ftype);

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

// ============================================================================
// Import Helpers: single-file scan + directory import
// ============================================================================

static void scan_file_and_collect_decls(Parser *p, const char *file_path) {
    size_t file_size = 0;
    char *source = zith_load_file_to_arena(p->arena, file_path, &file_size);
    if (!source || file_size == 0)
        return;

    ZithTokenStream tokens = zith_tokenize(p->arena, source, file_size);
    if (!tokens.data)
        return;

    Parser imp;
    parser_init(&imp, p->arena, source, file_size, file_path, tokens);
    imp.mode = ZITH_MODE_SCAN;

    ArenaList<ZithNode *> decls;
    decls.init(p->arena, 16);
    while (!parser_is_at_end(&imp)) {
        size_t pb = imp.pos;
        ZithNode *d = parser_parse_declaration(&imp);
        if (d)
            decls.push(p->arena, d);
        if (imp.pos == pb && !parser_is_at_end(&imp))
            parser_advance(&imp);
    }
    if (decls.size() > 0)
        parser_set_imported_decls(&decls, p->arena);
}

static void load_and_scan_module(Parser *p, const ZithSourceLoc loc,
                                  const std::string &root_dir,
                                  const std::string &rel_fs_path,
                                  int recurse_depth) {
    std::string file_path = root_dir + "/" + rel_fs_path + ".zith";
    if (zith_file_exists(file_path.c_str())) {
        scan_file_and_collect_decls(p, file_path.c_str());
        return;
    }

    // Not a file — try as directory
    std::string dir_path = root_dir + "/" + rel_fs_path;
    if (!zith_is_directory(dir_path.c_str()))
        return;

    try {
        namespace fs = std::filesystem;
        if (recurse_depth == 0) {
            for (auto &entry : fs::directory_iterator(dir_path)) {
                if (entry.is_regular_file() && zith_is_source_file(entry.path().string().c_str()))
                    scan_file_and_collect_decls(p, entry.path().string().c_str());
            }
        } else {
            int max_depth = (recurse_depth == -1) ? INT_MAX : recurse_depth;
            for (auto it = fs::recursive_directory_iterator(dir_path);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (it.depth() >= max_depth) {
                    it.disable_recursion_pending();
                    continue;
                }
                if (it->is_regular_file() && zith_is_source_file(it->path().string().c_str()))
                    scan_file_and_collect_decls(p, it->path().string().c_str());
            }
        }
    } catch (const std::exception &e) {
        parser_emit_diag(p, loc, ZITH_DIAG_ERROR, e.what());
    }
}


static bool split_import_path(Parser *p, const ZithSourceLoc loc,
                              const std::string &import_path,
                              std::string &root,
                              std::string &rel_path) {
    size_t slash_pos = import_path.find('/');
    if (slash_pos != std::string::npos) {
        root     = import_path.substr(0, slash_pos);
        rel_path = import_path.substr(slash_pos + 1);
        return true;
    }

    size_t dot_pos = import_path.find('.');
    if (dot_pos != std::string::npos) {
        if (!p->allow_dot_imports) {
            parser_emit_diag(p, loc, ZITH_DIAG_ERROR,
                             "dot-separated import paths are disabled; use '/' instead");
            root     = import_path;
            rel_path = "";
            return false;
        }
        root     = import_path.substr(0, dot_pos);
        rel_path = import_path.substr(dot_pos + 1);
        return true;
    }

    root     = import_path;
    rel_path = "";
    return true;
}

static bool resolve_import_root_dir(const Parser *p, const std::string &root,
                                    std::string &root_dir) {
    for (size_t i = 0; i < p->import_root_count; ++i) {
        const char *root_full = p->import_roots[i];
        const char *slash     = strrchr(root_full, '/');
        const char *root_name = slash ? slash + 1 : root_full;
        if (root == root_name) {
            root_dir = root_full;
            return true;
        }
    }
    return false;
}

static void scan_import_path_if_allowed(Parser *p, const ZithSourceLoc loc,
                                        const char *path, size_t path_len,
                                        int recurse_depth) {
    if (p->mode != ZITH_MODE_SCAN || !p->import_roots || p->import_root_count == 0)
        return;

    std::string import_path(path, path_len);
    std::string root;
    std::string rel_path;
    if (!split_import_path(p, loc, import_path, root, rel_path) || rel_path.empty())
        return;

    std::string root_dir;
    if (!resolve_import_root_dir(p, root, root_dir))
        return;

    std::string rel_fs_path;
    rel_fs_path.reserve(rel_path.size());
    for (char c : rel_path)
        rel_fs_path.push_back(c == '.' ? '/' : c);
    load_and_scan_module(p, loc, root_dir, rel_fs_path, recurse_depth);
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

    // Recursion specifier: (...), (N), or none
    int recurse_depth = 0;
    if (parser_match(p, ZITH_TOKEN_LPAREN)) {
        if (parser_match(p, ZITH_TOKEN_DOTS)) {
            recurse_depth = -1;
        } else {
            const ZithToken *num = parser_expect(p, ZITH_TOKEN_NUMBER, "expected recursion depth");
            recurse_depth = atoi(num->lexeme.data);
        }
        parser_expect(p, ZITH_TOKEN_RPAREN, "expected ')'");
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
                                 false,
                                 recurse_depth};
    register_import_symbol(p, buf, buf_len, ZITH_VIS_PRIVATE);

    scan_import_path_if_allowed(p, loc, buf, buf_len, recurse_depth);

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

    // Recursion specifier: (...), (N), or none
    int recurse_depth = 0;
    if (parser_match(p, ZITH_TOKEN_LPAREN)) {
        if (parser_match(p, ZITH_TOKEN_DOTS)) {
            recurse_depth = -1;
        } else {
            const ZithToken *num = parser_expect(p, ZITH_TOKEN_NUMBER, "expected recursion depth");
            recurse_depth = atoi(num->lexeme.data);
        }
        parser_expect(p, ZITH_TOKEN_RPAREN, "expected ')'");
    }

    parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");

    ZithImportPayload payload = {
        zith_arena_str(p->arena, module_buf, module_len),
        module_len,
        ZITH_VIS_PRIVATE,
        nullptr,
        0,
        false,
        true,
        recurse_depth
    };
    register_import_symbol(p, module_buf, module_len, ZITH_VIS_PRIVATE);

    scan_import_path_if_allowed(p, loc, module_buf, module_len, recurse_depth);

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
                                 false,
                                 0};
    register_import_symbol(p, buf, buf_len, ZITH_VIS_PUBLIC);

    // In SCAN mode, try to load and import the module
    scan_import_path_if_allowed(p, loc, buf, buf_len, 0);

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
    parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
    return expr;
}

#ifdef __cplusplus
void print_scanned_symbols() {
    printf("Scanned symbols from ModuleRegistry:\n");
    for (auto &registry = zith::import::ModuleRegistry::instance();
         const auto &mod_name : registry.list_modules()) {
        if (const auto mod = registry.get_module(mod_name)) {
            printf("  Module: %s\n", mod_name.c_str());
            for (const auto &sym : mod->symbols()) {
                const char *kind_str = "???";
                switch (sym.kind()) {
                    case zith::import::SymbolKind::Function: kind_str = "fn"; break;
                    case zith::import::SymbolKind::Struct: kind_str = "struct"; break;
                    case zith::import::SymbolKind::Trait: kind_str = "trait"; break;
                    case zith::import::SymbolKind::Enum: kind_str = "enum"; break;
                    case zith::import::SymbolKind::Module: kind_str = "import"; break;
                    default: kind_str = "unknown"; break;
                }
                const char *vis_str = "priv";
                if (sym.visibility() == zith::import::Visibility::Public) vis_str = "pub";
                else if (sym.visibility() == zith::import::Visibility::Module) vis_str = "mod";
                printf("    [%s] %s (%s)\n", kind_str, sym.name().c_str(), vis_str);
            }
        }
    }
}
#endif