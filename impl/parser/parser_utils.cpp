// impl/parser/parser_utils.cpp — Token navigation, error handling, synchronization
//
// Refactored to use v2 DiagnosticBag as the implementation,
// while maintaining v1 ZithDiagList ABI for backward compatibility.
#include "diagnostics/diagnostics.hpp"
#include "parser_context.hpp"
#include "memory/arena.hpp"
#include "zith/parser.h"
#include "zith/ast.h"
#include <cstdarg>
#include <cstring>

using zith::ArenaList;

extern ZithNode *parser_parse_statement(Parser *p);

// ============================================================================
// Parser Init
// ============================================================================

void parser_init(Parser *p, ZithArena *arena, const char *source, const size_t source_len,
                  const char *filename, const ZithTokenStream tokens) {
    p->arena              = arena;
    p->source             = source;
    p->source_len         = source_len;
    p->filename           = filename ? filename : "<input>";
    p->tokens             = tokens.data;
    p->count              = tokens.len;
    p->pos                = 0;
    p->had_error          = false;
    p->panic              = false;
    p->fn_kind            = ZITH_FN_NORMAL;
    p->inside_fn          = false;
    p->current_visibility = ZITH_VIS_PRIVATE;
    p->current_vis_depth  = 0;
    p->mode               = ZITH_MODE_SCAN;
    p->diags              = {nullptr, 0, 0};
    p->scan_root          = nullptr;
    p->import_roots        = nullptr;
    p->import_root_count   = 0;
    p->allow_dot_imports   = false;

    // Initialize v2 DiagManager — owned by ParseContext (tls_parse_ctx)
    auto &parse_ctx = get_tls_parse_ctx();
    parse_ctx.diag_manager = std::make_unique<DiagManager>();
    auto *dm = parse_ctx.diag_manager.get();
    dm->set_arena(arena);
    p->diag_manager = dm;

    // Register source file in the SourceMap for v2 emitter
    dm->source_map().add_or_get_file(p->filename, std::string_view(source, source_len));
}

void parser_destroy(Parser *p) {
    // DiagManager ownership is transferred to ParseContext in zith_parse_with_source
    // Do NOT delete it here — let ParseContext manage its lifecycle
    p->diag_manager = nullptr;
}

// ============================================================================
// Token Navigation (unchanged)
// ============================================================================

namespace {
static constexpr ZithToken static_eof = {{nullptr, 0}, {0, 0}, ZITH_TOKEN_END, 0};
}

const ZithToken *parser_peek(const Parser *p) {
    if (p->pos < p->count)
        return &p->tokens[p->pos];
    return &static_eof;
}

const ZithToken *parser_peek_ahead(const Parser *p, size_t offset) {
    size_t idx = p->pos + offset;
    if (idx < p->count)
        return &p->tokens[idx];
    return &static_eof;
}

const ZithToken *parser_advance(Parser *p) {
    const ZithToken *t = parser_peek(p);
    if (p->pos < p->count)
        p->pos++;
    return t;
}

bool parser_check(const Parser *p, ZithTokenType type) {
    return parser_peek(p)->type == type;
}

bool parser_match(Parser *p, ZithTokenType type) {
    if (parser_check(p, type)) {
        parser_advance(p);
        return true;
    }
    return false;
}

const ZithToken *parser_expect(Parser *p, ZithTokenType type, const char *msg) {
    if (parser_check(p, type))
        return parser_advance(p);

    // CRITICAL: Do not emit error if already in panic mode.
    if (p->panic)
        return parser_peek(p);

    const ZithToken *t = parser_peek(p);
    char buf[256];
    snprintf(buf, sizeof(buf), "%s (got '%.*s')", msg, static_cast<int>(t->lexeme.len), t->lexeme.data);
    parser_error(p, t->loc, buf);
    return t;
}

bool parser_is_at_end(const Parser *p) {
    return parser_peek(p)->type == ZITH_TOKEN_END;
}

bool parser_check_kw(const Parser *p, const char *kw) {
    const ZithToken *t = parser_peek(p);
    if (t->type != ZITH_TOKEN_IDENTIFIER)
        return false;
    size_t len = strlen(kw);
    return t->lexeme.len == len && memcmp(t->lexeme.data, kw, len) == 0;
}

// Legacy alias
bool check_kw(const Parser *p, const char *kw) {
    return parser_check_kw(p, kw);
}

// ============================================================================
// Error Handling & Synchronization
// ============================================================================

void parser_emit_diag(Parser *p, ZithSourceLoc loc, ZithDiagSeverity severity, const char *msg) {
    // v2: Emit to DiagnosticBag using DiagnosticBuilder
    auto *dm = static_cast<DiagManager*>(p->diag_manager);
    const zith::diag::FileId fid = dm->source_map().add_or_get_file(p->filename, std::string_view(p->source, p->source_len));

    using namespace zith::diag;
    DiagLevel level;
    DiagCode code = DiagCode::UnexpectedToken; // Generic fallback for legacy calls
    switch (severity) {
    case ZITH_DIAG_ERROR:   level = DiagLevel::Error; break;
    case ZITH_DIAG_WARNING: level = DiagLevel::Warning; code = DiagCode::DeprecatedSyntax; break;
    case ZITH_DIAG_NOTE:    level = DiagLevel::Note; break;
    default:                level = DiagLevel::Help; break;
    }

    SourceSpan span = SourceSpan::from_loc(loc, fid);
    DiagnosticBuilder(level, code, span)
        .with_raw_message(msg ? msg : "")
        .emit(dm->bag());

    // v1 ABI: Also populate legacy list for backward compatibility
    if (p->diags.count >= p->diags.capacity) {
        size_t new_cap = p->diags.capacity == 0 ? 8 : p->diags.capacity * 2;
        auto *buf      = static_cast<ZithDiagnostic *>(
            zith_arena_alloc(p->arena, new_cap * sizeof(ZithDiagnostic)));
        if (!buf)
            return;
        if (p->diags.items)
            memcpy(buf, p->diags.items, p->diags.count * sizeof(ZithDiagnostic));
        p->diags.items    = buf;
        p->diags.capacity = new_cap;
    }
    ZithDiagnostic d;
    d.message                        = zith_arena_strdup(p->arena, msg);
    d.loc                            = loc;
    d.severity                       = severity;
    p->diags.items[p->diags.count++] = d;
    if (severity == ZITH_DIAG_ERROR)
        p->had_error = true;
}

void parser_synchronize(Parser *p) {
    while (!parser_is_at_end(p)) {
        if (parser_check(p, ZITH_TOKEN_SEMICOLON)) {
            parser_advance(p);
            p->panic = false;
            return;
        }

        switch (parser_peek(p)->type) {
        case ZITH_TOKEN_FN:
        case ZITH_TOKEN_STRUCT:
        case ZITH_TOKEN_ENUM:
        case ZITH_TOKEN_IF:
        case ZITH_TOKEN_FOR:
        case ZITH_TOKEN_RETURN:
        case ZITH_TOKEN_LBRACE:
        case ZITH_TOKEN_RBRACE:
            p->panic = false;
            return;
        default:
            parser_advance(p);
            break;
        }
    }
}

void parser_error(Parser *p, ZithSourceLoc loc, const char *msg) {
    if (p->panic)
        return;

    parser_emit_diag(p, loc, ZITH_DIAG_ERROR, msg);
    p->panic = true;
    parser_synchronize(p);
}

void parser_warning(Parser *p, ZithSourceLoc loc, const char *msg) {
    parser_emit_diag(p, loc, ZITH_DIAG_WARNING, msg);
}

void parser_note(Parser *p, ZithSourceLoc loc, const char *msg) {
    parser_emit_diag(p, loc, ZITH_DIAG_NOTE, msg);
}

// ============================================================================
// New-style parser diagnostic helpers (using DiagnosticBuilder)
// ============================================================================

#if defined(__cplusplus) && __cplusplus >= 202002L

void parser_error_new(Parser *p, zith::diag::DiagCode code,
                       ZithSourceLoc loc, const char *fmt, ...) {
    if (p->panic) return;

    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    auto *dm = static_cast<DiagManager*>(p->diag_manager);
    const zith::diag::FileId fid = dm->source_map().add_or_get_file(p->filename, std::string_view(p->source, p->source_len));

    using namespace zith::diag;
    SourceSpan span = SourceSpan::from_loc(loc, fid);
    DiagnosticBuilder(DiagLevel::Error, code, span)
        .with_raw_message(buf)
        .emit(dm->bag());

    // v1 ABI: Also populate legacy list
    parser_emit_diag(p, loc, ZITH_DIAG_ERROR, buf);
    p->panic = true;
    parser_synchronize(p);
}

void parser_warning_new(Parser *p, zith::diag::DiagCode code,
                        const ZithSourceLoc loc, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    auto *dm = static_cast<DiagManager*>(p->diag_manager);
    const zith::diag::FileId fid = dm->source_map().add_or_get_file(p->filename, std::string_view(p->source, p->source_len));

    using namespace zith::diag;
    SourceSpan span = SourceSpan::from_loc(loc, fid);
    DiagnosticBuilder(DiagLevel::Warning, code, span)
        .with_raw_message(buf)
        .emit(dm->bag());

    // v1 ABI
    parser_emit_diag(p, loc, ZITH_DIAG_WARNING, buf);
}

void parser_note_new(Parser *p, ZithSourceLoc loc, const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    auto *dm = static_cast<DiagManager*>(p->diag_manager);
    const zith::diag::FileId fid = dm->source_map().add_or_get_file(p->filename, std::string_view(p->source, p->source_len));

    using namespace zith::diag;
    SourceSpan span = SourceSpan::from_loc(loc, fid);
    DiagnosticBuilder(DiagLevel::Note, DiagCode::UnexpectedToken, span)
        .with_raw_message(buf)
        .emit(dm->bag());

    // v1 ABI
    parser_emit_diag(p, loc, ZITH_DIAG_NOTE, buf);
}

#endif // __cplusplus >= 202002L

// ============================================================================
// Synchronization helpers (unchanged)
// ============================================================================

void skip_block(Parser *p) {
    if (!parser_match(p, ZITH_TOKEN_LBRACE)) {
        while (!parser_check(p, ZITH_TOKEN_SEMICOLON) && !parser_is_at_end(p))
            parser_advance(p);
        parser_expect(p, ZITH_TOKEN_SEMICOLON, "expected ';'");
        return;
    }

    int depth = 1;
    while (!parser_is_at_end(p) && depth > 0) {
        const ZithToken *t = parser_advance(p);
        if (t->type == ZITH_TOKEN_LBRACE)
            depth++;
        else if (t->type == ZITH_TOKEN_RBRACE)
            depth--;
    }
}

void parser_set_import_roots(Parser *p, const char **roots, size_t count) {
    p->import_roots      = roots;
    p->import_root_count = count;
}

void parser_set_allow_dot_imports(Parser *p, bool allow) {
    p->allow_dot_imports = allow;
}

// ============================================================================
// Ownership Helpers
// ============================================================================

ZithOwnership parser_ownership_from_node(const ZithNode *type_node) {
    if (!type_node)
        return ZITH_OWN_DEFAULT;
    switch (type_node->type) {
    case ZITH_NODE_TYPE_UNIQUE:    return ZITH_OWN_UNIQUE;
    case ZITH_NODE_TYPE_SHARED:    return ZITH_OWN_SHARED;
    case ZITH_NODE_TYPE_VIEW:      return ZITH_OWN_VIEW;
    case ZITH_NODE_TYPE_LEND:      return ZITH_OWN_LEND;
    case ZITH_NODE_TYPE_PACK:      return ZITH_OWN_EXTENSION;
    case ZITH_NODE_TYPE_EXTENSION: return ZITH_OWN_EXTENSION;
    default: return ZITH_OWN_DEFAULT;
    }
}

// ============================================================================
// List Node Helpers
// ============================================================================

ZithNode *parser_make_list_node(Parser *p, ZithSourceLoc loc,
                                        uint16_t type,
                                        ZithNode *(*parse_fn)(Parser *),
                                        const char *error_msg) {
    ArenaList<ZithNode *> items_b;
    items_b.init(p->arena, 8);
    while (!parser_check(p, ZITH_TOKEN_PIPE) && !parser_is_at_end(p)) {
        items_b.push(p->arena, parse_fn(p));
        if (!parser_match(p, ZITH_TOKEN_COMMA))
            break;
    }
    parser_expect(p, ZITH_TOKEN_PIPE, error_msg);
    size_t count    = 0;
    ZithNode **items = items_b.flatten(p->arena, &count);
    auto *n = static_cast<ZithNode *>(zith_arena_alloc(p->arena, sizeof(ZithNode)));
    if (n) {
        memset(n, 0, sizeof(ZithNode));
        n->type = type;
        n->loc  = loc;
        n->data.list.ptr = items;
        n->data.list.len = count;
    }
    return n;
}

ZithNode *parser_parse_block_body(Parser *p, bool expect_brace, ZithSourceLoc loc) {
    if (expect_brace)
        parser_expect(p, ZITH_TOKEN_LBRACE, "expected '{'");
    else if (!parser_match(p, ZITH_TOKEN_LBRACE))
        return nullptr;
    ArenaList<ZithNode *> stmts_b;
    stmts_b.init(p->arena, 16);
    while (!parser_check(p, ZITH_TOKEN_RBRACE) && !parser_is_at_end(p))
        stmts_b.push(p->arena, parser_parse_statement(p));
    parser_expect(p, ZITH_TOKEN_RBRACE, "expected '}'");
    size_t count     = 0;
    ZithNode **stmts = stmts_b.flatten(p->arena, &count);
    return zith_ast_make_block(p->arena, loc, stmts, count);
}

// ============================================================================
// Literals (unchanged)
// ============================================================================

ZithLiteral parse_lit_number(const char *data, size_t len, ZithTokenType type) {
    char buf[64];
    if (len >= sizeof(buf))
        return {ZITH_LIT_INT, {.i64 = 0}};
    memcpy(buf, data, len);
    buf[len]        = '\0';
    ZithLiteral lit = {};
    if (type == ZITH_TOKEN_HEXADECIMAL) {
        lit.kind      = ZITH_LIT_UINT;
        lit.value.u64 = (uint64_t)strtoull(buf, nullptr, 16);
    } else if (type == ZITH_TOKEN_BINARY) {
        lit.kind     = ZITH_LIT_UINT;
        uint64_t val = 0;
        for (size_t i = 2; i < len; ++i)
            if (data[i] == '0' || data[i] == '1')
                val = (val << 1) | (data[i] - '0');
        lit.value.u64 = val;
    } else if (type == ZITH_TOKEN_OCTAL) {
        lit.kind      = ZITH_LIT_UINT;
        lit.value.u64 = (uint64_t)strtoull(buf + 2, nullptr, 8);
    } else {
        for (size_t i = 0; i < len; ++i)
            if (buf[i] == '.') {
                lit.kind      = ZITH_LIT_FLOAT;
                lit.value.f64 = strtod(buf, nullptr);
                return lit;
            }
        lit.kind      = ZITH_LIT_INT;
        lit.value.i64 = (int64_t)strtoll(buf, nullptr, 10);
    }
    return lit;
}
