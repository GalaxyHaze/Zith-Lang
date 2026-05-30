#include "recovery.hpp"

namespace zith::frontend::parser::recovery {

    void panic(lexer::TokenStream& stream, SyncSet sync_tokens) {
        while (!stream.is_empty()) {
            for (auto tk : sync_tokens) {
                if (stream.peek().is(tk)) return;
            }
            stream.advance();
        }
    }

    bool isSync(lexer::TokenKind kind) {
        switch (kind) {
            case lexer::TokenKind::End:
            case lexer::TokenKind::Punctuation:
                return true;
            default:
                return false;
        }
    }

}
