#pragma once

#include "lexer/token.hpp"

#include <initializer_list>

namespace zith::frontend::parser::recovery {

    using SyncSet = std::initializer_list<lexer::TokenKind>;

    void panic(lexer::TokenStream &stream, SyncSet sync_tokens);

    bool isSync(lexer::TokenKind kind);

} // namespace zith::frontend::parser::recovery
