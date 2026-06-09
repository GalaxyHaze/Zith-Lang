#include "lexer.hpp"

#include "diagnostics/error-codes.hpp"
#include "lexer/keyword-table.hpp"
#include "memory/source-file.hpp"
#include "memory/source-map.hpp"
#include "memory/span.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace zith::lexer {

    bool Lexer::isNum(char c) {
        return c >= '0' && c <= '9';
    }

    bool Lexer::isPunctuation(char c) {
        switch (c) {
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case ':':
            case ';':
            case ',':
            case '.':
            case '"':
            case '\'':
            case '\\':
            case '#':
            case '@':
            case '`':
                return true;
            default:
                return false;
        }
    }

    bool Lexer::isOperator(char c) {
        switch (c) {
            case '+':
            case '-':
            case '*':
            case '/':
            case '%':
            case '=':
            case '<':
            case '>':
            case '!':
            case '&':
            case '|':
            case '^':
            case '~':
            case '?':
                return true;
            default:
                return false;
        }
    }

    bool Lexer::isOpen() const {
        return now < end;
    }

    char Lexer::peek() const {
        return (now + 1 <= end) ? now[1] : '\0';
    }

    char Lexer::peek(size_t n) const {
        return (now + n <= end) ? now[n] : '\0';
    }

    bool Lexer::consume(size_t offset) {
        if (end - now < offset)
            return false;
        now += offset;
        return true;
    }

    bool Lexer::match(std::string_view must) {
        if (end - now < must.size())
            return false;
        std::string_view rest{now, end};
        if (rest.starts_with(must)) {
            now += must.size();
            return true;
        }
        return false;
    }

    memory::Span Lexer::spanAt(const char *p) const noexcept {
        auto off = static_cast<uint32_t>(p - start);
        return {gId, off, off + 1};
    }

    memory::Span Lexer::spanRange(const char *b, const char *e) const noexcept {
        return {gId,
                static_cast<uint32_t>(b - start),
                static_cast<uint32_t>(e - start)};
    }

    void Lexer::singleComment() {
        const auto before = now - 2;
        while (isOpen()) {
            if (*now == '\n') {
                loc.col = 1;
                loc.line++;
                break;
            }
            loc.col++;
            now++;
        }
        tokens.emplace(spanRange(before, now), TokenKind::Comments);
    }

    void Lexer::singleDoc() {
        const auto before = now - 3;
        while (isOpen()) {
            if (*now == '\n') {
                loc.col = 1;
                loc.line++;
                break;
            }
            loc.col++;
            now++;
        }
        tokens.emplace(spanRange(before, now), TokenKind::Docs);
    }

    void Lexer::multiComment() {
        const auto before = now - 2;
        while (isOpen()) {
            if (*now == '*' && peek() == '/') {
                now += 2;
                tokens.emplace(spanRange(before, now), TokenKind::Comments);
                return;
            }
            if (*now == '\n') {
                loc.col = 1;
                loc.line++;
            } else {
                loc.col++;
            }
            now++;
        }
        diags_.report(diagnostics::Severity::Error, diagnostics::err::UnclosedString,
                       "Unterminated block comment '/*'", spanRange(before, now));
    }

    void Lexer::multiDoc() {
        const auto before = now - 3;
        while (isOpen()) {
            if (*now == '*' && peek() == '/') {
                now += 2;
                tokens.emplace(spanRange(before, now), TokenKind::Docs);
                return;
            }
            if (*now == '\n') {
                loc.col = 1;
                loc.line++;
            } else {
                loc.col++;
            }
            now++;
        }
        diags_.report(diagnostics::Severity::Error, diagnostics::err::UnclosedString,
                       "Unterminated doc comment '/**'", spanRange(before, now));
    }

    void Lexer::processNumber() {
        const auto before = now;

        if (*now == '0' && now + 1 < end) {
            const char nxt = *(now + 1);
            if (nxt == 'x' || nxt == 'X') {
                now += 2;
                while (now < end && (isNum(*now) || (*now >= 'a' && *now <= 'f') ||
                                     (*now >= 'A' && *now <= 'F'))) {
                    loc.col++;
                    now++;
                }
                if (now == before + 2) {
                    diags_.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                                   "Invalid hex literal: expected digits after '0x'",
                                   spanAt(before));
                }
                tokens.emplace(spanRange(before, now), TokenKind::LitVal);
                return;
            }
            if (nxt == 'c' || nxt == 'C') {
                now += 2;
                while (now < end && *now >= '0' && *now <= '7') {
                    loc.col++;
                    now++;
                }
                if (now == before + 2) {
                    diags_.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                                   "Invalid octal literal: expected digits after '0c'",
                                   spanAt(before));
                }
                tokens.emplace(spanRange(before, now), TokenKind::LitVal);
                return;
            }
            if (nxt == 'b' || nxt == 'B') {
                now += 2;
                while (now < end && (*now == '0' || *now == '1')) {
                    loc.col++;
                    now++;
                }
                if (now == before + 2) {
                    diags_.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                                   "Invalid binary literal: expected digits after '0b'",
                                   spanAt(before));
                }
                tokens.emplace(spanRange(before, now), TokenKind::LitVal);
                return;
            }
        }

        while (now < end && isNum(*now)) {
            loc.col++;
            now++;
        }

        if (now < end && *now == '.' && now + 1 < end && isNum(*(now + 1))) {
            now++;
            loc.col++;
            while (now < end && isNum(*now)) {
                loc.col++;
                now++;
            }
        }

        tokens.emplace(spanRange(before, now), TokenKind::LitVal);
    }

    void Lexer::processString() {
        const auto before = now;
        const char quote  = *now;
        now++;
        loc.col++;

        while (isOpen()) {
            if (*now == quote) {
                now++;
                loc.col++;
                tokens.emplace(spanRange(before, now), TokenKind::LitVal);
                return;
            }
            if (*now == '\\') {
                now++;
                loc.col++;
                if (!isOpen())
                    break;
            }
            if (*now == '\n') {
                loc.line++;
                loc.col = 1;
            } else {
                loc.col++;
            }
            now++;
        }

        diags_.report(diagnostics::Severity::Error, diagnostics::err::UnclosedString,
                       "Unterminated string literal", spanRange(before, now));
    }

    void Lexer::processIdentifier() {
        const auto before = now;
        while (now < end && (std::isalnum(*now) || *now == '_')) {
            loc.col++;
            now++;
        }
        std::string_view word{before, static_cast<size_t>(now - before)};
        TokenKind kind = lookup_keyword(word);
        tokens.emplace(spanRange(before, now), kind);
    }

    Lexer::Lexer(diagnostics::DiagnosticEngine &diags) : diags_(diags), tokens(memory::SessionArena) {}

    auto Lexer::run(std::variant<memory::FileId, std::pair<std::string_view, std::string>> input)
            -> memory::Result<TokenStream> {

        if (auto *id = std::get_if<memory::FileId>(&input)) {
            gId = *id;
        } else {
            auto &[name, content] = std::get<std::pair<std::string_view, std::string>>(input);
            if (auto i = memory::SourceMap::add_file(name, content)) {
                gId = i.value();
            }
        }

        if (auto i = memory::SourceMap::get(gId)) {
            file = &i.value().get();
        }

        if (!file)
            return memory::Error{"[error] Couldnt find the file"};

        const auto content = file->getSlice();
        start = now = content.data();
        end         = content.data() + content.size();
        loc         = {1, 1};

        while (isOpen()) {
            if (std::isspace(*now)) {
                if (*now == '\n') {
                    loc.line++;
                    loc.col = 1;
                } else {
                    loc.col++;
                }
                now++;
                continue;
            }

            if (match("///")) {
                singleDoc();
                continue;
            }

            if (match("//")) {
                singleComment();
                continue;
            }

            if (match("/**")) {
                multiDoc();
                continue;
            }

            if (match("/*")) {
                multiComment();
                continue;
            }

            if (*now == '"' || *now == '\'') {
                processString();
                continue;
            }

            if (isNum(*now)) {
                processNumber();
                continue;
            }

            if (std::isalpha(*now) || *now == '_') {
                processIdentifier();
                continue;
            }

            if (isOperator(*now)) {
                const auto before = now;
                loc.col++;
                now++;
                tokens.emplace(spanRange(before, now), TokenKind::Operators);
                continue;
            }

            if (isPunctuation(*now)) {
                const auto before = now;
                loc.col++;
                now++;
                tokens.emplace(spanRange(before, now), TokenKind::Punctuation);
                continue;
            }

            {
                const auto before = now;
                loc.col++;
                now++;
                auto span = spanRange(before, now);
                diags_.report(diagnostics::Severity::Error, diagnostics::err::UnknownToken,
                               std::string("Unexpected character '") + *before + "'", span);
                tokens.emplace(span, TokenKind::Unknown);
            }
        }

        tokens.emplace(
                spanRange(now, now),
                TokenKind::End);

        if (diags_.hasErrors())
            return memory::Error{"tokenization failed — see diagnostics for details"};

        return TokenStream{tokens.data(), static_cast<uint32_t>(tokens.size()), 0};
    }

    auto tokenize(memory::FileId id, diagnostics::DiagnosticEngine &diags) -> memory::Result<TokenStream> {
        Lexer lexer(diags);
        return lexer.run(std::variant<memory::FileId, std::pair<std::string_view, std::string>>(id));
    }

    auto tokenize(std::string_view name, std::string content, diagnostics::DiagnosticEngine &diags) -> memory::Result<TokenStream> {
        Lexer lexer(diags);
        return lexer.run(std::make_pair(name, std::move(content)));
    }

    const char *tokenKindName(TokenKind k) noexcept {
        switch (k) {
            case TokenKind::Identifier:
                return "Identifier";
            case TokenKind::As:
                return "As";
            case TokenKind::Using:
                return "Using";
            case TokenKind::Type:
                return "Type";
            case TokenKind::Struct:
                return "Struct";
            case TokenKind::Raw:
                return "Raw";
            case TokenKind::Must:
                return "Must";
            case TokenKind::Mutable:
                return "Mutable";
            case TokenKind::Trait:
                return "Trait";
            case TokenKind::Interface:
                return "Interface";
            case TokenKind::Typedef:
                return "Typedef";
            case TokenKind::Implement:
                return "Implement";
            case TokenKind::Fn:
                return "Fn";
            case TokenKind::Module:
                return "Module";
            case TokenKind::Extern:
                return "Extern";
            case TokenKind::Macro:
                return "Macro";
            case TokenKind::Context:
                return "Context";
            case TokenKind::Variable:
                return "Variable";
            case TokenKind::Ownership:
                return "Ownership";
            case TokenKind::Yield:
                return "Yield";
            case TokenKind::Label:
                return "Label";
            case TokenKind::Visibility:
                return "Visibility";
            case TokenKind::If:
                return "If";
            case TokenKind::For:
                return "For";
            case TokenKind::In:
                return "In";
            case TokenKind::Match:
                return "Match";
            case TokenKind::Control:
                return "Control";
            case TokenKind::Scene:
                return "Scene";
            case TokenKind::Thread:
                return "Thread";
            case TokenKind::Error:
                return "Error";
            case TokenKind::Drop:
                return "Drop";
            case TokenKind::Require:
                return "Require";
            case TokenKind::Is:
                return "Is";
            case TokenKind::Word:
                return "Word";
            case TokenKind::Logical:
                return "Logical";
            case TokenKind::Comparison:
                return "Comparison";
            case TokenKind::Operators:
                return "Operators";
            case TokenKind::Comments:
                return "Comments";
            case TokenKind::Docs:
                return "Docs";
            case TokenKind::Annotation:
                return "Annotation";
            case TokenKind::Punctuation:
                return "Punctuation";
            case TokenKind::LitVal:
                return "LitVal";
            case TokenKind::Unknown:
                return "Unknown";
            case TokenKind::End:
                return "End";
        }
        return "???";
    }

    void printTokens(const TokenStream &stream) noexcept {
        for (uint32_t i = 0; i < stream.len; ++i) {
            const auto &tok = stream.src[i];
            auto res        = memory::SourceMap::snippet(tok.span);
            auto lexeme     = res.isOk() ? res.value() : std::string_view{};
            printf("  %-16s \"%.*s\"  [%u..%u]\n",
                   tokenKindName(tok.kind),
                   (int)lexeme.size(),
                   lexeme.data(),
                   tok.span.start,
                   tok.span.end);
        }
    }

} // namespace zith::lexer
