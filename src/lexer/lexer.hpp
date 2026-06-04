#pragma once

#include "lexer/token.hpp"
#include "memory/dyn-array.hpp"
#include "memory/result.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace zith::parser {
    struct SourceLoc;
}

namespace zith::lexer {

    class Lexer {
            parser::SourceLoc *file   = nullptr;
            parser::FileId gId        = 0;
            const char *start = nullptr;
            const char *now   = nullptr;
            const char *end   = nullptr;
            parser::Loc loc{};

            enum class ErrorKind : uint8_t {
                Success = 0,
                UnexpectedToken,
                MissingSemicolon,
                InvalidNumber,
                UnexpectedEOF
            };

            ErrorKind err = ErrorKind::Success;
            memory::DynArray<Token> tokens;

            static bool isNum(char c);
            static bool isPunctuation(char c);
            static bool isOperator(char c);
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
            auto run(std::variant<parser::FileId, std::pair<std::string_view, std::string>> input)
                    -> memory::Result<TokenStream>;
        };

        auto tokenize(parser::FileId id) -> memory::Result<TokenStream>;

        auto tokenize(std::string_view, std::string) -> memory::Result<TokenStream>;

        const char *tokenKindName(TokenKind k) noexcept;

        void printTokens(const TokenStream &stream) noexcept;

} // namespace zith::lexer
