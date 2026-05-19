// impl/parser/parser.cpp — Parser entry point and pipeline orchestration
#include "zith/parser.h"
#include "diagnostics/diagnostics.hpp"

#include "memory/utils.hpp"
#include "import/module_registry.hpp"

#include <cstring>
#include <vector>

using zith::ArenaList;

// Global storage for imported declarations - cleared each parse
static std::vector<ZithNode *> g_imported_decls_vec;
static int g_parser_depth = 0;
static ZithDiagList g_last_diags = {nullptr, 0, 0};
static DiagManager *g_last_diag_manager = nullptr;

bool g_import_loaded_this_file = false;

void parser_set_imported_decls(void *decls, ZithArena *arena) {
    auto *src        = static_cast<ArenaList<ZithNode *> *>(decls);
    size_t count     = 0;
    ZithNode **items = src->flatten(arena, &count);
    for (size_t i = 0; i < count; ++i) {
        g_imported_decls_vec.push_back(items[i]);
    }
    g_import_loaded_this_file = true;
}

void *parser_get_imported_decls() {
    return &g_imported_decls_vec;
}

void parser_enter_scope() {
    g_parser_depth++;
}
void parser_exit_scope() {
    g_parser_depth--;
    if (g_parser_depth == 0) {
        g_imported_decls_vec.clear();
    }
}

static ZithNode *expand_unbody(Parser *parent, ZithNode *node);

static ZithNode *run_parser_phase(Parser *p, ZithParserMode mode) {
    p->pos       = 0;
    p->panic     = false;
    p->had_error = false;
    p->mode      = mode;
    parser_enter_scope();

    if (mode == ZITH_MODE_SCAN) {
        // Clear ModuleRegistry for new scan phase
        zith::import::ModuleRegistry::instance().clear();
    }

    ArenaList<ZithNode *> decls_b;
    decls_b.init(p->arena, 16);

    while (!parser_is_at_end(p)) {
        size_t pos_before = p->pos;
        ZithNode *decl    = parser_parse_declaration(p);
        if (decl)
            decls_b.push(p->arena, decl);
        if (p->pos == pos_before && !parser_is_at_end(p))
            parser_advance(p);
    }

    // Imported functions are registered in SEMA's ctx.functions, not added to AST

    size_t count     = 0;
    ZithNode **decls = decls_b.flatten(p->arena, &count);
    return zith_ast_make_program(p->arena, decls, count);
}

static ZithNode *expand_unbody(Parser *parent, ZithNode *node) {
    if (!node)
        return nullptr;

    if (node->type == ZITH_NODE_UNBODY) {
        const auto *body_tokens = static_cast<const ZithToken *>(node->data.list.ptr);
        const size_t body_len   = node->data.list.len;

        auto *tokens = static_cast<ZithToken *>(
            zith_arena_alloc(parent->arena, sizeof(ZithToken) * (body_len + 1)));
        if (!tokens)
            return node;
        if (body_len)
            memcpy(tokens, body_tokens, sizeof(ZithToken) * body_len);
        tokens[body_len] = {{nullptr, 0}, node->loc, ZITH_TOKEN_END, 0};

        Parser inner{};
        parser_init(&inner, parent->arena, parent->source, parent->source_len, parent->filename,
                    {tokens, body_len + 1});
        inner.mode = ZITH_MODE_EXPAND;

        ArenaList<ZithNode *> stmts_b;
        stmts_b.init(parent->arena, 16);
        while (!parser_is_at_end(&inner)) {
            size_t before  = inner.pos;
            ZithNode *stmt = parser_parse_statement(&inner);
            if (stmt)
                stmts_b.push(parent->arena, stmt);
            if (inner.pos == before && !parser_is_at_end(&inner))
                parser_advance(&inner);
        }
        size_t count     = 0;
        ZithNode **stmts = stmts_b.flatten(parent->arena, &count);

        // Merge inner diagnostics into parent using v2 system
        auto *inner_dm = static_cast<DiagManager*>(inner.diag_manager);
        auto *parent_dm = static_cast<DiagManager*>(parent->diag_manager);
        for (const auto& diag : inner_dm->bag().diagnostics()) {
            parent_dm->emit_diagnostic(Diagnostic{
                diag.level, diag.code, diag.message,
                diag.primary_span, diag.has_primary_span,
                diag.secondary_spans, diag.suggestions, {}, diag.caused_by
            });
        }
        // Also copy legacy list for ABI compat
        for (size_t i = 0; i < inner.diags.count; ++i)
            parser_emit_diag(parent, inner.diags.items[i].loc, inner.diags.items[i].severity,
                             inner.diags.items[i].message);

        parser_destroy(&inner);
        return zith_ast_make_block(parent->arena, node->loc, stmts, count);
    }

    if (node->type == ZITH_NODE_FUNC_DECL) {
        auto *fn = static_cast<ZithFuncPayload *>(node->data.list.ptr);
        fn->body = expand_unbody(parent, fn->body);
    } else if (node->type == ZITH_NODE_BLOCK || node->type == ZITH_NODE_PROGRAM) {
        auto **items = static_cast<ZithNode **>(node->data.list.ptr);
        for (size_t i = 0; i < node->data.list.len; ++i)
            items[i] = expand_unbody(parent, items[i]);
    } else if (node->type == ZITH_NODE_IF) {
        node->data.kids.b = expand_unbody(parent, node->data.kids.b);
        node->data.kids.c = expand_unbody(parent, node->data.kids.c);
    }

    return node;
}

ZithNode *zith_parse_with_source(ZithArena *arena, const char *source, size_t source_len,
                                 const char *filename, ZithTokenStream tokens,
                                 const char **import_roots, size_t import_root_count) {
    Parser p;
    parser_init(&p, arena, source, source_len, filename, tokens);
    parser_set_import_roots(&p, import_roots, import_root_count);

    ZithNode *scan_root = run_parser_phase(&p, ZITH_MODE_SCAN);
    p.scan_root         = scan_root;

    extern void print_scanned_symbols();
    print_scanned_symbols();

    ZithNode *expanded = expand_unbody(&p, scan_root);

    extern void sema_run(Parser * p, ZithNode * root);
    sema_run(&p, expanded);

    // Clear imported decls after sema
    g_imported_decls_vec.clear();
    g_parser_depth = 0;

    // Finalize v2 diagnostics
    auto *dm = static_cast<DiagManager*>(p.diag_manager);
    dm->bag().finalize();

    // Store diagnostics for LSP access (v1 ABI)
    g_last_diags = p.diags;
    g_last_diag_manager = dm;

    // Print using v2 TerminalEmitter
    dm->emitter().set_source_map(&dm->source_map());
    dm->emitter().emit(dm->bag(), stderr);

    // Sync had_error from v2 bag
    p.had_error = p.had_error || dm->had_error();

    // Note: DiagManager is NOT destroyed here — g_last_diag_manager keeps it alive
    // for LSP access. It will be replaced on next parse call.
    // Clean up previous global manager if any (leaked intentionally for now to avoid UAF)

    if (p.had_error)
        return nullptr;
    return expanded;
}

ZithDiagList *zith_get_parse_diagnostics(void) {
    return &g_last_diags;
}

// v2: Get the DiagManager for direct access (LSP, etc.)
DiagManager *zith_get_parse_diag_manager(void) {
    return g_last_diag_manager;
}
