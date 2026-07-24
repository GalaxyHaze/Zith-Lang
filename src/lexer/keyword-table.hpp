#pragma once
#include "lexer/token.hpp"

#include <array>
#include <cstdint>

namespace zith::lexer {

namespace detail {

constexpr uint64_t hash64(std::string_view sv) {
    uint64_t h = 14695981039346656037ULL;
    for (auto c : sv) {
        h ^= static_cast<uint8_t>(c);
        h *= 1099511628211ULL;
    }
    return h;
}

constexpr uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static constexpr auto TokenTable = std::to_array<std::pair<std::string_view, TokenKind>>({

    // Types → Keyword
    {"i8",      Keyword{KwType{}}},
    {"i16",     Keyword{KwType{}}},
    {"i32",     Keyword{KwType{}}},
    {"i64",     Keyword{KwType{}}},
    {"i128",    Keyword{KwType{}}},

    {"u8",      Keyword{KwType{}}},
    {"u16",     Keyword{KwType{}}},
    {"u32",     Keyword{KwType{}}},
    {"u64",     Keyword{KwType{}}},
    {"u128",    Keyword{KwType{}}},

    {"f32",     Keyword{KwType{}}},
    {"f64",     Keyword{KwType{}}},

    {"bool",    Keyword{KwType{}}},
    {"char",    Keyword{KwType{}}},
    {"void",    Keyword{KwType{}}},
    {"invalid", Keyword{KwType{}}},
    {"unknown", Keyword{KwType{}}},
    {"never",   Keyword{KwType{}}},

    // Literals
    {"null",  Literal{LitSub::Null}},
    {"true",  Literal{LitSub::Bool}},
    {"false", Literal{LitSub::Bool}},

    // Declaration
    {"type",      Declaration{DeclSub::Typedef}},
    {"struct",    Declaration{DeclSub::Struct}},
    {"component", Declaration{DeclSub::Struct}},
    {"enum",      Declaration{DeclSub::Enum}},
    {"union",     Declaration{DeclSub::Union}},
    {"macro",     Declaration{DeclSub::Macro}},

    {"let",    Declaration{DeclSub::Let}},
    {"var",    Declaration{DeclSub::Var}},
    {"auto",   Declaration{DeclSub::Auto}},
    {"const",  Declaration{DeclSub::Const}},
    {"global", Declaration{DeclSub::Global}},

    {"word", Declaration{DeclSub::Word}},

    // Fn
    {"fn",    Fn{FnSub::Default}},
    {"flow",  Fn{FnSub::Flow}},

    // Interface
    {"trait",     Interface{IfaceSub::Trait}},
    {"interface", Interface{IfaceSub::Interface}},
    {"implement", Interface{IfaceSub::Implement}},
    {"extends",   Interface{IfaceSub::Extends}},
    {"dyn",       Interface{IfaceSub::Dyn}},

    // Block
    {"marker",  Block{BlkSub::Marker}},
    {"drop",    Block{BlkSub::Drop}},
    {"dock",    Block{BlkSub::Dock}},
    {"fail",   Block{BlkSub::Fail}},
    {"with",   Block{BlkSub::With}},
    {"catch",  Block{BlkSub::Catch}},
    {"context", Block{BlkSub::Context}},
    {"use",    Block{BlkSub::Use}},

    // Keyword
    {"as",     Keyword{KwAs{}}},
    {"raw",    Keyword{KwRaw{}}},
    {"unsafe", Keyword{KwRaw{}}},
    {"must",   Keyword{KwMust{}}},
    {"mut",    Keyword{KwMutable{}}},
    {"import", Keyword{KwImport{}}},
    {"export", Keyword{KwExport{}}},
    {"from",   Keyword{KwFrom{}}},
    {"alias",  Keyword{KwAlias{}}},
    {"pub",    Keyword{KwVisibility{}}},
    {"mod",    Keyword{KwVisibility{}}},
    {"if",     Keyword{KwIf{}}},
    {"else",   Keyword{KwIf{}}},
    {"for",    Keyword{KwFor{}}},
    {"in",     Keyword{KwIn{}}},
    {"when",   Keyword{KwWhen{}}},
    {"return", Keyword{KwControl{}}},
    {"break",  Keyword{KwControl{}}},
    {"continue", Keyword{KwControl{}}},
    {"jump",   Keyword{KwControl{}}},
    {"while",  Keyword{KwControl{}}},
    {"throw",  Keyword{KwThrow{}}},
    {"require", Keyword{KwRequire{}}},
    {"is",     Keyword{KwIs{}}},
    {"spawn",  Keyword{KwSpawn{}}},
    {"await",  Keyword{KwAwait{}}},
    {"prefix", Keyword{KwWord{}}},
    {"suffix", Keyword{KwWord{}}},
    {"infix",  Keyword{KwWord{}}},
    {"nop",    Keyword{KwWord{}}},
    {"and",    Keyword{KwLogical{}}},
    {"or",     Keyword{KwLogical{}}},
    {"not",    Keyword{KwLogical{}}},
    {"xor",    Keyword{KwLogical{}}},

    // Ownership
    {"lend",   Keyword{Ownership{Ownership::Sub::Lend}}},
    {"share",  Keyword{Ownership{Ownership::Sub::Share}}},
    {"view",   Keyword{Ownership{Ownership::Sub::View}}},
    {"unique", Keyword{Ownership{Ownership::Sub::Unique}}},
    {"belong", Keyword{Ownership{Ownership::Sub::Belong}}},

    // Module keywords
    {"module", Keyword{KwModule{}}},
    {"using",  Keyword{KwUsing{}}},
});

static constexpr size_t N           = TokenTable.size();
static constexpr size_t BucketCount = 128;
static constexpr size_t TableSize   = 256;

struct PerfectHash {
    std::array<int16_t, TableSize> table{};
    std::array<uint8_t, BucketCount> bucketSeed{};

    constexpr PerfectHash() {
        table.fill(-1);
        bucketSeed.fill(0);

        std::array<uint8_t, BucketCount> counts{};
        std::array<std::array<uint16_t, 16>, BucketCount> items{};

        for (size_t i = 0; i < N; ++i) {
            const size_t b        = hash64(TokenTable[i].first) % BucketCount;
            items[b][counts[b]++] = static_cast<uint16_t>(i);
        }

        for (size_t b = 0; b < BucketCount; ++b) {
            if (counts[b] == 0)
                continue;
            for (uint8_t seed = 0; seed < 255; ++seed) {
                bool ok = true;
                std::array<size_t, 16> slots{};
                for (size_t k = 0; k < counts[b] && ok; ++k) {
                    const size_t i    = items[b][k];
                    const size_t slot = mix64(hash64(TokenTable[i].first) ^ seed) % TableSize;
                    for (size_t j = 0; j < k; ++j) {
                        if (slots[j] == slot) {
                            ok = false;
                            break;
                        }
                    }
                    if (!ok)
                        break;
                    if (table[slot] != -1) {
                        ok = false;
                        break;
                    }
                    slots[k] = slot;
                }
                if (ok) {
                    bucketSeed[b] = seed;
                    for (size_t k = 0; k < counts[b]; ++k) {
                        const size_t i    = items[b][k];
                        const size_t slot = mix64(hash64(TokenTable[i].first) ^ seed) % TableSize;
                        table[slot]       = static_cast<int16_t>(i);
                    }
                    break;
                }
            }
        }
    }

    [[nodiscard]] constexpr TokenKind lookup(std::string_view sv) const {
        if (sv.empty())
            return Identifier{};

        const uint64_t h = hash64(sv);
        const size_t b   = h % BucketCount;
        const size_t idx = mix64(h ^ bucketSeed[b]) % TableSize;
        const int16_t id = table[idx];

        if (id < 0)
            return Identifier{};
        return (TokenTable[static_cast<size_t>(id)].first == sv)
                   ? TokenTable[static_cast<size_t>(id)].second
                   : Identifier{};
    }
};

constexpr auto g_hasher = PerfectHash{};

constexpr bool hasDuplicateKeywords() noexcept {
    for (size_t i = 0; i < N; ++i)
        for (size_t j = i + 1; j < N; ++j)
            if (TokenTable[i].first == TokenTable[j].first)
                return true;
    return false;
}
static_assert(!hasDuplicateKeywords(), "TokenTable contains duplicate keywords");

constexpr bool hasBucketOverflow() noexcept {
    std::array<uint8_t, BucketCount> counts{};
    for (size_t i = 0; i < N; ++i) {
        const size_t b = hash64(TokenTable[i].first) % BucketCount;
        if (++counts[b] > 16)
            return true;
    }
    return false;
}
static_assert(!hasBucketOverflow(), "Bucket overflow (>16 keywords per bucket)");

constexpr bool allKeywordsPlaced() noexcept {
    for (size_t i = 0; i < N; ++i) {
        const uint64_t h  = hash64(TokenTable[i].first);
        const size_t b    = h % BucketCount;
        const size_t slot = mix64(h ^ g_hasher.bucketSeed[b]) % TableSize;
        const int16_t id  = g_hasher.table[slot];
        if (id < 0)
            return false;
        if (TokenTable[static_cast<size_t>(id)].first != TokenTable[i].first)
            return false;
    }
    return true;
}
static_assert(allKeywordsPlaced(), "Not all keywords placed in the perfect hash table");

} // namespace detail

inline TokenKind lookupKeyword(std::string_view sv) {
    return detail::g_hasher.lookup(sv);
}

} // namespace zith::lexer
