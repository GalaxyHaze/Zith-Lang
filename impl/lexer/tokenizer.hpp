// impl/lexer/tokenizer.hpp — Tokenizer internal types (C++)
#pragma once

#include "zith/token.h"
#include "memory/utils.hpp"
#include <string_view>
#include <vector>

namespace zith::lexer {

struct LexError {
    const char *msg;
    ZithSourceLoc info;
};

using TokenList = ArenaList<ZithToken>;

ZithTokenStream tokenize(std::string_view source, ZithArena *arena, TokenList &tokens,
                         std::vector<LexError> &errors);

void debug_tokenize(std::string_view source, ZithArena *arena, TokenList &tokens,
                    std::vector<LexError> &errors);

}
