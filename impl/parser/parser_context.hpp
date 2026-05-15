// impl/parser/parser_context.hpp — C++ ParserContext wrapper
#pragma once

#include "zith/parser.h"

#ifdef __cplusplus
#include "zith/zith.hpp"

void print_scanned_symbols();

class ParserContext {
public:
    ParserContext() : parser_{} {}

    void init(zith::Arena &arena, const char *source, size_t source_len,
              const char *filename, ZithTokenStream tokens) {
        arena_ = &arena;
        diag_.set_arena(arena.get());
        parser_init(&parser_, arena.get(), source, source_len, filename, tokens);
    }

    Parser &parser() { return parser_; }
    const Parser &parser() const { return parser_; }

    DiagManager &diag() { return diag_; }
    const DiagManager &diag() const { return diag_; }

    zith::Arena *arena() { return arena_; }
    const zith::Arena *arena() const { return arena_; }

private:
    Parser parser_;
    DiagManager diag_;
    zith::Arena *arena_ = nullptr;
};

#endif
