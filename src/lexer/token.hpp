#pragma once
#include "memory/span.hpp"

#include <cstdint>
#include <string_view>


namespace zith::lexer {

    enum class TokenKind : uint8_t {
        Identifier,
        As, // as
        Using,
        Type,
        Struct,
        Raw,
        Must,
        Mutable,
        Trait,
        Interface,
        Typedef,
        Implement,
        Fn, // fn, const fn, flowing fn. raw fn
        Module,
        Extern,
        Macro, // macro, raw macro, tag macro
        Context,
        Variable,
        Ownership, //
        Yield,
        Label,
        Visibility,
        If,
        For,
        In,
        Match,
        Control,
        Scene,
        Thread,
        Error,
        Drop,
        Require,
        Is,
        Word,
        Logical,
        Comparison,
        Operators,
        Comments,
        Docs,
        Annotation,
        Punctuation,
        LitVal,
        Unknown,
        End
    };

    struct Token {
        // lexeme
        zith::memory::Span span;
        // type of token
        TokenKind kind;
        // single char for Punctuation/Operators (0 otherwise)
        char punc = 0;

        [[nodiscard]] constexpr bool is(TokenKind k) const noexcept {
            return kind == k;
        }
        [[nodiscard]] constexpr bool is_not(TokenKind k) const noexcept {
            return kind != k;
        }
        [[nodiscard]] constexpr bool is_eof() const noexcept {
            return kind == TokenKind::End;
        }
    };

    struct TokenStream {
        Token *src      = nullptr;
        uint32_t len    = 0;
        uint32_t offset = 0;
        const char *file_base = nullptr;

        [[nodiscard]] std::string_view lexeme(const Token &t) const noexcept {
            return {file_base + t.span.start, t.span.end - t.span.start};
        }
        [[nodiscard]] std::string_view lexeme() const noexcept {
            return lexeme(peek());
        }

        // Retorna o token atual sem avançar o ponteiro (Lookahead)
        [[nodiscard]] constexpr auto peek() const noexcept -> const Token & {
            if (offset >= len) [[unlikely]] {
                // Retorna o último elemento (que idealmente deve ser TokenKind::End)
                return src[len - 1];
            }
            return src[offset];
        }

        // Retorna um token à frente baseado em uma distância (Lookahead arbitrário: peek(1),
        // peek(2))
        [[nodiscard]] constexpr auto peek(uint32_t lookahead) const noexcept -> const Token & {
            uint32_t target = offset + lookahead;
            if (target >= len) [[unlikely]] {
                return src[len - 1];
            }
            return src[target];
        }

        // Avança o fluxo em 1 token
        constexpr void advance() noexcept {
            if (offset < len) [[likely]] {
                offset++;
            }
        }

        // Avança o fluxo em N tokens
        constexpr void advance(uint32_t n) noexcept {
            offset = (offset + n > len) ? len : offset + n;
        }

        // Verifica se o token atual é do tipo esperado e o avança.
        // Caso contrário, não faz nada e retorna falso (Útil para branching no parser)
        constexpr bool match(TokenKind kind) noexcept {
            if (peek().is(kind)) {
                advance();
                return true;
            }
            return false;
        }

        // Verifica se chegamos ao fim dos tokens válidos
        [[nodiscard]] constexpr bool is_empty() const noexcept {
            return offset >= len || peek().is_eof();
        }
    };
} // namespace zith::lexer
