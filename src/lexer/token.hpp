#pragma once
#include "common/token-utils.hpp"
#include "common/span.hpp"

#include <cstdint>
#include <string_view>
#include <variant>

namespace zith::lexer {

// ── Sub-enums ────────────────────────────────────────────────────────────────

enum class DeclSub : uint8_t {
    Let, Var, Global, Const, Auto,
    Struct, Enum, Union, Component,
    Typedef, Macro, Word
};

enum class LitSub : uint8_t {
    Null, Bool, Decimal, Hex, Oct, Bin, Float, String, Char
};

enum class FnSub : uint8_t {
    Default, Raw, Const, Extern, Flow
};

enum class IfaceSub : uint8_t {
    Trait, Interface, Implement, Extends, Dyn
};

enum class BlkSub : uint8_t {
    Marker, Drop, Dock, Fail, With, Catch, Context, Use
};

// ── KwSub (variant, because Ownership has nested data) ───────────────────────

#define ZITH_TAG(NAME)                                                             \
    struct NAME {                                                                  \
        constexpr auto operator==(const NAME &) const noexcept -> bool = default; \
    };

ZITH_TAG(KwAs)
ZITH_TAG(KwUsing)
ZITH_TAG(KwRaw)
ZITH_TAG(KwMust)
ZITH_TAG(KwMutable)
ZITH_TAG(KwIf)
ZITH_TAG(KwFor)
ZITH_TAG(KwIn)
ZITH_TAG(KwWhen)
ZITH_TAG(KwControl)
ZITH_TAG(KwRequire)
ZITH_TAG(KwIs)
ZITH_TAG(KwWord)
ZITH_TAG(KwLogical)
ZITH_TAG(KwModule)
ZITH_TAG(KwImport)
ZITH_TAG(KwExport)
ZITH_TAG(KwFrom)
ZITH_TAG(KwAlias)
ZITH_TAG(KwType)
ZITH_TAG(KwSpawn)
ZITH_TAG(KwAwait)
ZITH_TAG(KwThrow)
ZITH_TAG(KwVisibility)

#undef ZITH_TAG

struct Ownership {
    enum class Sub : uint8_t { Lend, Share, View, Unique, Belong };
    Sub sub;
    constexpr auto operator==(const Ownership &) const noexcept -> bool = default;
};

using KwSub = std::variant<
    KwAs, KwUsing, KwRaw, KwMust, KwMutable, KwVisibility,
    KwIf, KwFor, KwIn, KwWhen, KwControl,
    KwRequire, KwIs, KwWord, KwLogical,
    KwModule, KwImport, KwExport, KwFrom, KwAlias,
    KwType, KwSpawn, KwAwait, KwThrow,
    Ownership
>;

// ── Top-level token categories ───────────────────────────────────────────────

struct Identifier {
    constexpr auto operator==(const Identifier &) const noexcept -> bool = default;
};

struct Declaration {
    DeclSub sub;
    constexpr auto operator==(const Declaration &) const noexcept -> bool = default;
};

struct Keyword {
    KwSub sub;
    constexpr auto operator==(const Keyword &) const noexcept -> bool = default;
};

struct Literal {
    LitSub sub;
    constexpr auto operator==(const Literal &) const noexcept -> bool = default;
};

struct Punc {
    char c;
    constexpr auto operator==(const Punc &) const noexcept -> bool = default;
};

struct Operator {
    char c;
    constexpr auto operator==(const Operator &) const noexcept -> bool = default;
};

struct Fn {
    FnSub sub;
    constexpr auto operator==(const Fn &) const noexcept -> bool = default;
};

struct Interface {
    IfaceSub sub;
    constexpr auto operator==(const Interface &) const noexcept -> bool = default;
};

struct Block {
    BlkSub sub;
    constexpr auto operator==(const Block &) const noexcept -> bool = default;
};

struct Comments {
    constexpr auto operator==(const Comments &) const noexcept -> bool = default;
};

struct Annotation {
    constexpr auto operator==(const Annotation &) const noexcept -> bool = default;
};

struct Unknown {
    constexpr auto operator==(const Unknown &) const noexcept -> bool = default;
};

struct End {
    constexpr auto operator==(const End &) const noexcept -> bool = default;
};

using TokenKind = std::variant<
    Identifier, Declaration, Keyword, Literal, Punc, Operator,
    Fn, Interface, Block, Comments, Annotation, Unknown, End
>;

// ── Token ────────────────────────────────────────────────────────────────────

struct Token {
    memory::Span span;
    TokenKind kind;
};

inline constexpr Token kEndToken{{}, End{}};

// ── TokenStream ──────────────────────────────────────────────────────────────

struct TokenStream {
    Token *src            = nullptr;
    uint32_t len          = 0;
    uint32_t offset       = 0;
    const char *file_base = nullptr;

    [[nodiscard]] std::string_view lexeme(const Token &t) const noexcept {
        return {file_base + t.span.start, t.span.end - t.span.start};
    }
    [[nodiscard]] std::string_view lexeme() const noexcept {
        return lexeme(peek());
    }

    [[nodiscard]] constexpr auto peek() const noexcept -> const Token & {
        if (len == 0 || offset >= len) [[unlikely]]
            return kEndToken;
        return src[offset];
    }

    [[nodiscard]] constexpr auto peek(uint32_t lookahead) const noexcept -> const Token & {
        uint32_t target = offset + lookahead;
        if (len == 0 || target >= len) [[unlikely]]
            return kEndToken;
        return src[target];
    }

    constexpr void advance() noexcept {
        if (offset < len) [[likely]]
            offset++;
    }

    constexpr void advance(uint32_t n) noexcept {
        offset = (offset + n > len) ? len : offset + n;
    }

    template <typename T>
    constexpr bool match() noexcept {
        if (std::holds_alternative<T>(peek().kind)) {
            advance();
            return true;
        }
        return false;
    }

    template <typename T, typename F>
    constexpr bool match(F pred) noexcept {
        if (auto *p = std::get_if<T>(&peek().kind); p && pred(*p)) {
            advance();
            return true;
        }
        return false;
    }

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return offset >= len || std::holds_alternative<End>(peek().kind);
    }
};

} // namespace zith::lexer
