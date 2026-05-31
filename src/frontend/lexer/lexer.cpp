#include "lexer.hpp"

#include "frontend/lexer/keyword-table.hpp"
#include "frontend/source/source-file.hpp"
#include "frontend/source/source-map.hpp"
#include "frontend/source/span.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace zith::frontend::lexer {

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

    std::string Lexer::getErrorMsg() const {
        std::string locationPrefix = "[error]: at" + file->path + ":" + loc.toString() + ": ";
        switch (err) {
            case ErrorKind::Success:
                return "";
            case ErrorKind::UnexpectedToken:
                return locationPrefix + "Unexpected token found.";
            case ErrorKind::MissingSemicolon:
                return locationPrefix + "Missing semicolon ';'.";
            case ErrorKind::InvalidNumber:
                return locationPrefix + "Invalid number format.";
            case ErrorKind::UnexpectedEOF:
                return locationPrefix + "Unexpected End of File (EOF).";
        }
        return locationPrefix + "Unknown parser error.";
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
        tokens.emplace(Span{gId,
                            static_cast<uint32_t>(before - start),
                            static_cast<uint32_t>(now - start)},
                       TokenKind::Comments);
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
        tokens.emplace(Span{gId,
                            static_cast<uint32_t>(before - start),
                            static_cast<uint32_t>(now - start)},
                       TokenKind::Docs);
    }

    void Lexer::multiComment() {
        const auto before = now - 2;
        while (isOpen()) {
            if (*now == '*' && peek() == '/') {
                now += 2;
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::Comments);
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
        err = ErrorKind::UnexpectedEOF;
    }

    void Lexer::multiDoc() {
        const auto before = now - 3;
        while (isOpen()) {
            if (*now == '*' && peek() == '/') {
                now += 2;
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::Docs);
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
        err = ErrorKind::UnexpectedEOF;
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
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::LitVal);
                return;
            }
            if (nxt == 'c' || nxt == 'C') {
                now += 2;
                while (now < end && *now >= '0' && *now <= '7') {
                    loc.col++;
                    now++;
                }
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::LitVal);
                return;
            }
            if (nxt == 'b' || nxt == 'B') {
                now += 2;
                while (now < end && (*now == '0' || *now == '1')) {
                    loc.col++;
                    now++;
                }
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::LitVal);
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

        tokens.emplace(Span{gId,
                            static_cast<uint32_t>(before - start),
                            static_cast<uint32_t>(now - start)},
                       TokenKind::LitVal);
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
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::LitVal);
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

        err = ErrorKind::UnexpectedEOF;
    }

    void Lexer::processIdentifier() {
        const auto before = now;
        while (now < end && (std::isalnum(*now) || *now == '_')) {
            loc.col++;
            now++;
        }
        std::string_view word{before, static_cast<size_t>(now - before)};
        TokenKind kind = lookup_keyword(word);
        tokens.emplace(Span{gId,
                            static_cast<uint32_t>(before - start),
                            static_cast<uint32_t>(now - start)},
                       kind);
    }

    Lexer::Lexer() : tokens(infra::alloc::SessionArena) {}

    auto Lexer::run(std::variant<FileId, std::pair<std::string_view, std::string>> input)
            -> infra::util::Result<TokenStream> {
        err = ErrorKind::Success;

        if (auto *id = std::get_if<FileId>(&input)) {
            gId = *id;
        } else {
            auto &[name, content] = std::get<std::pair<std::string_view, std::string>>(input);
            if (auto i = SourceMap::add_file(name, content)) {
                gId = i.value();
            }
        }

        if (auto i = SourceMap::get(gId)) {
            file = &i.value().get();
        }

        if (!file)
            return infra::util::Error{"[error] Couldnt find the file"};

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

            if (isPunctuation(*now)) {
                const auto before = now;
                loc.col++;
                now++;
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::Punctuation);
                continue;
            }

            {
                const auto before = now;
                loc.col++;
                now++;
                tokens.emplace(Span{gId,
                                    static_cast<uint32_t>(before - start),
                                    static_cast<uint32_t>(now - start)},
                               TokenKind::Unknown);
            }
        }

        tokens.emplace(
                Span{gId, static_cast<uint32_t>(now - start), static_cast<uint32_t>(now - start)},
                TokenKind::End);

        if (err != ErrorKind::Success)
            return infra::util::Error{getErrorMsg()};

        return TokenStream{tokens.data(), static_cast<uint32_t>(tokens.size()), 0};
    }

    auto tokenize(FileId id) -> infra::util::Result<TokenStream> {
        Lexer lexer;
        return lexer.run(std::variant<FileId, std::pair<std::string_view, std::string>>(id));
    }

    auto tokenize(std::string_view name, std::string test) -> infra::util::Result<TokenStream> {
        Lexer lexer;
        return lexer.run(std::make_pair(name, std::move(test)));
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
            case TokenKind::Comparision:
                return "Comparision";
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
            auto res        = SourceMap::snippet(tok.span);
            auto lexeme     = res.isOk() ? res.value() : std::string_view{};
            printf("  %-16s \"%.*s\"  [%u..%u]\n",
                   tokenKindName(tok.kind),
                   (int)lexeme.size(),
                   lexeme.data(),
                   tok.span.start,
                   tok.span.end);
        }
    }

} // namespace zith::frontend::lexer
