#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "lexer/token.hpp"
#include "memory/dyn-array.hpp"
#include "memory/result.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace zith::memory {
    struct SourceLoc;
}

namespace zith::lexer {

    class Lexer {
            memory::SourceLoc *file   = nullptr;
            memory::FileId gId        = 0;
            const char *start = nullptr;
            const char *now   = nullptr;
            const char *end   = nullptr;
            memory::Loc loc{};

            diagnostics::DiagnosticEngine &diags_;
            memory::DynArray<Token> tokens;

            static bool isNum(char c);
            static bool isPunctuation(char c);
            static bool isOperator(char c);
            bool isOpen() const;
            char peek() const;
            char peek(size_t n) const;
            bool consume(size_t offset = 1);
            bool match(std::string_view must);
            void singleComment();
            void singleDoc();
            void multiComment();
            void multiDoc();
            void processNumber();
            void processString();
            void processIdentifier();

            memory::Span spanAt(const char *p) const noexcept;
            memory::Span spanRange(const char *b, const char *e) const noexcept;

        public:
            explicit Lexer(diagnostics::DiagnosticEngine &diags);
            auto run(std::variant<memory::FileId, std::pair<std::string_view, std::string>> input)
                    -> memory::Result<TokenStream>;
        };

        [[nodiscard]] auto tokenize(memory::FileId id, diagnostics::DiagnosticEngine &diags) -> memory::Result<TokenStream>;

        auto tokenize(std::string_view, std::string, diagnostics::DiagnosticEngine &diags) -> memory::Result<TokenStream>;

        const char *tokenKindName(TokenKind k) noexcept;

        void printTokens(const TokenStream &stream) noexcept;

} // namespace zith::lexer
