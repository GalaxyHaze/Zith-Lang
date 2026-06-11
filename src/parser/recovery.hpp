#pragma once

#include "lexer/token.hpp"

#include <initializer_list>

namespace zith::parser::recovery {

using SyncSet = std::initializer_list<lexer::TokenKind>;

inline bool isSync(lexer::TokenKind kind) {
    switch (kind) {
    case lexer::TokenKind::End:
    case lexer::TokenKind::Punctuation:
        return true;
    default:
        return false;
    }
}

inline void panic(lexer::TokenStream &stream, SyncSet sync_tokens) {
    while (!stream.is_empty()) {
        if (isSync(stream.peek().kind))
            return;
        stream.advance();
    }
}

} // namespace zith::parser::recovery
