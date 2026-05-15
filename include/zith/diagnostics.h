// include/zith/diagnostics.h — Diagnostic system (C ABI)
#pragma once

#include <stddef.h>
#include "zith/token.h"

#ifdef __cplusplus
extern "C" {
#endif

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

void zith_diag_print_all(const ZithDiagList *diags,
                         const char *source, size_t source_len,
                         const char *filename);

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
