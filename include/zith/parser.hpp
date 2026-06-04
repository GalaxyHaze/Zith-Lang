// include/zith/parser.hpp — C++ wrapper for Zith parser
#pragma once

#ifndef __cplusplus
#error "This header requires C++"
#endif

#include "zith/memory.h"
#include "zith/parser.h"

#include <cstdarg>

// Forward declarations for zith::diag types
namespace zith::diag { enum class DiagCode : uint32_t; }

#if defined(__GNUC__) || defined(__clang__)
#define ZITH_PRINTF_ATTR(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define ZITH_PRINTF_ATTR(fmt_idx, arg_idx)
#endif

class DiagManager;
DiagManager *zith_get_parse_diag_manager(void);

void parser_error_new(Parser *p, zith::diag::DiagCode code,
                       ZithSourceLoc loc, const char *fmt, ...)
    ZITH_PRINTF_ATTR(4, 5);
void parser_warning_new(Parser *p, zith::diag::DiagCode code,
                         ZithSourceLoc loc, const char *fmt, ...)
    ZITH_PRINTF_ATTR(4, 5);
void parser_note_new(Parser *p, ZithSourceLoc loc, const char *fmt, ...)
    ZITH_PRINTF_ATTR(3, 4);

#undef ZITH_PRINTF_ATTR

// Forward: defined in parser_test.cpp
ZithArena *zith_test_arena_get(void);

struct ParseResult {
    ZithNode *node = nullptr;

    ParseResult() = default;
    explicit ParseResult(ZithNode *n) : node(n) {}

    ~ParseResult() { reset(); }

    ParseResult(const ParseResult &)            = delete;
    ParseResult &operator=(const ParseResult &) = delete;

    ParseResult(ParseResult &&o) noexcept : node(o.node) { o.node = nullptr; }
    ParseResult &operator=(ParseResult &&o) noexcept {
        if (this != &o) { reset(); node = o.node; o.node = nullptr; }
        return *this;
    }

    ZithNode *get() const { return node; }
    ZithNode *operator->() const { return node; }
    explicit operator bool() const { return node != nullptr; }

    void reset() {
        if (node) {
            ZithArena *ta = zith_test_arena_get();
            if (ta) zith_arena_reset(ta);
            node = nullptr;
        }
    }
};

inline ParseResult parse_test(const char *source) {
    return ParseResult(zith_parse_test(source));
}

static inline void parser_emit(Parser *p, ZithSourceLoc loc, ZithDiagSeverity severity,
                               const char *msg) {
    parser_emit_diag(p, loc, severity, msg);
}
