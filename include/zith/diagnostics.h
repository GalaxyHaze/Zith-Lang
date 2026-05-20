// include/zith/diagnostics.h — Diagnostic system (C ABI)
// Legacy interface (v1) + new type forward declarations (v2)
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "zith/token.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// v1 (Legacy) Types — kept for backward compatibility
// ============================================================================

typedef enum ZithDiagSeverity {
    ZITH_DIAG_ERROR   = 0,
    ZITH_DIAG_WARNING = 1,
    ZITH_DIAG_NOTE    = 2,
    ZITH_DIAG_INFO    = 3,
} ZithDiagSeverity;

typedef struct ZithDiagnostic {
    const char *message;
    ZithSourceLoc loc;
    ZithDiagSeverity severity;
} ZithDiagnostic;

typedef struct ZithDiagList {
    ZithDiagnostic *items;
    size_t count;
    size_t capacity;
} ZithDiagList;

// ============================================================================
// v2 (New) Types — internal, used by C++ implementation
// ============================================================================

typedef uint32_t ZithFileId;

typedef struct ZithSourceSpan {
    ZithSourceLoc start;
    ZithSourceLoc end;
    ZithFileId file_id;
} ZithSourceSpan;

// Opaque handle to a C++ DiagnosticBag (for C API consumers that link C++)
typedef struct ZithDiagBag ZithDiagBag;

// ============================================================================
// v1 (Legacy) C API — DEPRECATED, use zith_diag_bag_print instead
// ============================================================================

void zith_diag_print_all(const ZithDiagList *diags,
                         const char *source, size_t source_len,
                         const char *filename);

// ============================================================================
// v2 (New) C API — forward declarations for C++ callers only
// These functions require linking C++ runtime.
// ============================================================================

// Create/destroy a diagnostic bag
ZithDiagBag *zith_diag_bag_create(void);
void         zith_diag_bag_destroy(ZithDiagBag *bag);

// Emit a simple diagnostic (message + severity + location)
void zith_diag_bag_emit(ZithDiagBag *bag,
                        const char *message,
                        ZithSourceSpan span,
                        int severity,   // 0=fatal, 1=bug, 2=error, 3=warning
                        uint32_t code);

// Finalize (sort, suppress cascading, cap)
void zith_diag_bag_finalize(ZithDiagBag *bag);

// Print to stderr
void zith_diag_bag_print(const ZithDiagBag *bag);

// Get error/warning counts
int  zith_diag_bag_had_errors(const ZithDiagBag *bag);
size_t zith_diag_bag_error_count(const ZithDiagBag *bag);
size_t zith_diag_bag_warning_count(const ZithDiagBag *bag);

// Get diagnostics as JSON string (for LSP/tooling)
// Returns a newly allocated string; caller must free() it.
// Returns NULL if no diagnostics or bag is NULL.
char *zith_diag_bag_get_json(const ZithDiagBag *bag);

// ============================================================================
// Debug/Utility
// ============================================================================

#ifndef ZITH_NO_DEBUG
void debug_print(const char *fmt, ...);
void debug_println(const char *fmt, ...);
void debug_error(const char *fmt, ...);
#else
static inline void debug_print(const char *fmt, ...) { (void)fmt; }
static inline void debug_println(const char *fmt, ...) { (void)fmt; }
static inline void debug_error(const char *fmt, ...) { (void)fmt; }
#endif

void zith_io_error(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
