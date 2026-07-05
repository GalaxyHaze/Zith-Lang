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
    memory::SourceMap &source_map_;
    memory::Arena &arena_;
    memory::SourceLoc *file = nullptr;
    memory::FileId gId      = 0;
    const char *start       = nullptr;
    const char *now         = nullptr;
    const char *end         = nullptr;

    diagnostics::DiagnosticEngine &diags_;
    memory::DynArray<Token> tokens;

    static bool isNum(char c);
    static bool isPunctuation(char c);
    static bool isOperator(char c);
    bool isOpen() const;
    char peek() const;
    char peek(size_t n) const;
    bool match(std::string_view must);
    void singleLineComment(size_t prefixLen, TokenKind kind);
    void multiLineComment(size_t prefixLen, TokenKind kind);
    void processNumber();
    void processString();
    void processIdentifier();

    memory::Span spanAt(const char *p) const noexcept;
    memory::Span spanRange(const char *b, const char *e) const noexcept;

public:
    Lexer(memory::SourceMap &source_map, memory::Arena &arena,
          diagnostics::DiagnosticEngine &diags);
    auto run(std::variant<memory::FileId, std::pair<std::string_view, std::string>> input)
        -> memory::Result<TokenStream>;
};

[[nodiscard]] auto tokenize(memory::SourceMap &source_map, memory::Arena &arena, memory::FileId id,
                            diagnostics::DiagnosticEngine &diags) -> memory::Result<TokenStream>;

auto tokenize(memory::SourceMap &source_map, memory::Arena &arena, std::string_view, std::string,
              diagnostics::DiagnosticEngine &diags) -> memory::Result<TokenStream>;

const char *tokenKindName(TokenKind k) noexcept;

void printTokens(const TokenStream &stream) noexcept;

} // namespace zith::lexer
