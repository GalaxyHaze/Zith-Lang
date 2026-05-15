// include/zith/parser.hpp — C++ wrapper for Zith parser
#pragma once

#ifndef __cplusplus
#error "This header requires C++"
#endif

#include "zith/parser.h"

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
