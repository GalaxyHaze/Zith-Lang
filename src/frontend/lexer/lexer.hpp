#pragma once

#include "frontend/lexer/token.hpp"
#include "infra/alloc/arena.hpp"
#include "infra/collections/dyn-array.hpp"
#include "infra/util/result.hpp"

namespace zith::frontend {

struct SourceLoc;

namespace lexer {

class Lexer {
    SourceLoc* file = nullptr;
    FileId gId = 0;
    const char *start = nullptr;
    const char *now = nullptr;
    const char *end = nullptr;
    Loc loc{1, 1};

    enum class ErrorKind : uint8_t {
        Success = 0,
        UnexpectedToken,
        MissingSemicolon,
        InvalidNumber,
        UnexpectedEOF
    };

    ErrorKind err = ErrorKind::Success;
    infra::collections::DynArray<Token> tokens;

    static bool isNum(char c);
    static bool isPunctuation(char c);
    bool isOpen() const;
    char peek() const;
    char peek(size_t n) const;
    bool consume(size_t offset = 1);
    bool match(std::string_view must);
    std::string getErrorMsg() const;
    void singleComment();
    void singleDoc();
    void multiComment();
    void multiDoc();
    void processNumber();
    void processString();
    void processIdentifier();

public:
    Lexer();
    auto run(FileId id) -> infra::util::Result<TokenStream>;
};

auto tokenize(FileId id) -> infra::util::Result<TokenStream>;

const char* tokenKindName(TokenKind k) noexcept;

void printTokens(const TokenStream& stream, std::string_view source) noexcept;

}

}
