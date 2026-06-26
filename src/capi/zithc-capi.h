#ifndef ZITHC_CAPI_H
#define ZITHC_CAPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Opaque handle ─────────────────────────────────────────────────────
typedef struct zithc_session zithc_session;

// ── Severity levels (matches diagnostics::Severity) ───────────────────
typedef enum {
    ZITHC_SEVERITY_NOTE    = 0,
    ZITHC_SEVERITY_WARNING = 1,
    ZITHC_SEVERITY_ERROR   = 2,
    ZITHC_SEVERITY_BUG     = 3
} zithc_severity;

// ── Pipeline stages (matches cli::Stage) ──────────────────────────────
typedef enum {
    ZITHC_STAGE_SOURCE          = 0,
    ZITHC_STAGE_LEXED           = 1,
    ZITHC_STAGE_SCANNED         = 2,
    ZITHC_STAGE_IMPORTED        = 3,
    ZITHC_STAGE_RESOLVED        = 4,
    ZITHC_STAGE_TYPE_CHECKED    = 5,
    ZITHC_STAGE_SOLVED          = 6,
    ZITHC_STAGE_NRA_RESOLVED    = 7,
    ZITHC_STAGE_HIR_LOWERED     = 8,
    ZITHC_STAGE_MIR_LOWERED     = 9,
    ZITHC_STAGE_ZIR_INTERPRETED = 10
} zithc_stage;

// ── Position (0-based line/col, as LSP expects) ───────────────────────
typedef struct {
    uint32_t line;
    uint32_t col;
} zithc_position;

// ── Source span ───────────────────────────────────────────────────────
typedef struct {
    uint32_t start;
    uint32_t end;
} zithc_span;

// ── Diagnostic info ───────────────────────────────────────────────────
typedef struct {
    zithc_severity severity;
    uint32_t code;
    const char *message;
    zithc_span span;
} zithc_diagnostic;

// ── Session lifecycle ─────────────────────────────────────────────────
zithc_session *zithc_session_create(const char *file_path);
zithc_session *zithc_session_create_from_buffer(const char *uri, const char *content,
                                                size_t length);
void zithc_session_destroy(zithc_session *session);

// ── Configuration ─────────────────────────────────────────────────────
void zithc_session_add_include_dir(zithc_session *session, const char *dir);

// ── Pipeline control ──────────────────────────────────────────────────
bool zithc_run(zithc_session *session);
bool zithc_run_to(zithc_session *session, int stage);

// ── Diagnostics ───────────────────────────────────────────────────────
size_t zithc_diag_count(zithc_session *session);
zithc_diagnostic zithc_diag_get(zithc_session *session, size_t index);
size_t zithc_diag_suggestion_count(zithc_session *session, size_t diag_index);
const char *zithc_diag_suggestion_get(zithc_session *session, size_t diag_index, size_t sug_index);
bool zithc_has_errors(zithc_session *session);
void zithc_emit_diagnostics(zithc_session *session);

// ── Hover ──────────────────────────────────────────────────────────────
const char *zithc_hover(zithc_session *session, uint32_t offset);

// ── JSON result (for WASM/playground) ──────────────────────────────────
const char *zithc_run_to_json(zithc_session *session, int stage);

// ── Position conversion ───────────────────────────────────────────────
zithc_position zithc_offset_to_position(zithc_session *session, uint32_t offset);

// ── Error info ────────────────────────────────────────────────────────
const char *zithc_last_error(zithc_session *session);

#ifdef __cplusplus
}
#endif

#endif // ZITHC_CAPI_H
