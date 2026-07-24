#include "lexer.hpp"

#include "diagnostics/error-codes.hpp"
#include "lexer/keyword-table.hpp"
#include "memory/source-file.hpp"
#include "memory/source-map.hpp"
#include "common/span.hpp"

#include <cstdint>
#include <cstdio>
#include <string_view>

namespace zith::lexer {

static bool isAsciiSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static bool isAsciiAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool isAsciiAlnum(char c) {
    return isAsciiAlpha(c) || (c >= '0' && c <= '9');
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
    case '#':
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

bool Lexer::match(std::string_view must) {
    if (static_cast<size_t>(end - now) < must.size())
        return false;
    std::string_view rest(now, static_cast<size_t>(end - now));
    if (rest.starts_with(must)) {
        now += must.size();
        return true;
    }
    return false;
}

void Lexer::singleLineComment(bool isDoc) {
    const auto before = now - (isDoc ? 3 : 2);
    while (isOpen()) {
        if (*now == '\n')
            break;
        now++;
    }
    tokens.emplace(Token{spanRange(before, now), Comments{}});
}

void Lexer::multiLineComment(bool isDoc) {
    const auto before = now - (isDoc ? 3 : 2);
    const char *delim = (isDoc) ? "/**" : "/*";
    while (isOpen()) {
        if (*now == '*' && peek() == '/') {
            now += 2;
            tokens.emplace(Token{spanRange(before, now), Comments{}});
            return;
        }
        now++;
    }
    diags.report(diagnostics::Severity::Error, diagnostics::err::UnclosedComment,
                 std::string("Unterminated block comment '") + delim + "'", spanRange(before, now));
}

static bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

void Lexer::processNumber() {
    const auto before = now;
    bool isFloat = false;

    if (*now == '0' && now + 1 < end) {
        char nxt = now[1];

        if (nxt == 'x' || nxt == 'X') {
            now += 2;
            while (now < end && isHexDigit(*now))
                now++;
            if (now == before + 2) {
                diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                             "Invalid hex literal: expected digits after '0x'", spanAt(before));
            }
            tokens.emplace(Token{spanRange(before, now), Literal{LitSub::Hex}});
            return;
        }
        if (nxt == 'c' || nxt == 'C') {
            now += 2;
            while (now < end && *now >= '0' && *now <= '7')
                now++;
            if (now == before + 2) {
                if (now < end && isNum(*now)) {
                    diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                                 "Invalid octal digit in literal", spanAt(now));
                } else {
                    diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                                 "Invalid octal literal: expected digits after '0c'",
                                 spanAt(before));
                }
            } else if (now < end && isNum(*now)) {
                diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                             "Invalid octal digit in literal", spanAt(now));
            }
            tokens.emplace(Token{spanRange(before, now), Literal{LitSub::Oct}});
            return;
        }
        if (nxt == 'b' || nxt == 'B') {
            now += 2;
            while (now < end && (*now == '0' || *now == '1'))
                now++;
            if (now == before + 2) {
                diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidIntLiteral,
                             "Invalid binary literal: expected digits after '0b'", spanAt(before));
            }
            tokens.emplace(Token{spanRange(before, now), Literal{LitSub::Bin}});
            return;
        }
    }

    while (now < end && isNum(*now))
        now++;

    if (now < end && *now == '.' && now + 1 < end && isNum(*(now + 1))) {
        isFloat = true;
        now++;
        while (now < end && isNum(*now))
            now++;
    }

    tokens.emplace(Token{spanRange(before, now), Literal{isFloat ? LitSub::Float : LitSub::Decimal}});
}

void Lexer::processString() {
    const auto before = now;
    const char quote  = *now;
    bool isChar = (quote == '\'');
    now++;

    while (isOpen()) {
        if (*now == quote) {
            now++;
            tokens.emplace(Token{spanRange(before, now), Literal{isChar ? LitSub::Char : LitSub::String}});
            return;
        }
        if (*now == '\\') {
            now++;
            if (!isOpen()) {
                diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidEscape,
                             "Unexpected end of file in escape sequence", spanAt(now - 1));
                tokens.emplace(Token{spanRange(before, now), Literal{isChar ? LitSub::Char : LitSub::String}});
                return;
            }
            if (*now == '\n' || *now == '\r') {
                if (*now == '\r' && isOpen() && *(now + 1) == '\n')
                    now++;
                now++;
                while (isOpen() && (*now == ' ' || *now == '\t' || *now == '\n' || *now == '\r'))
                    now++;
                continue;
            }
            char esc = *now;
            if (esc != 'n' && esc != 't' && esc != 'r' && esc != '0' && esc != '\\' && esc != '"' &&
                esc != '\'') {
                diags.report(diagnostics::Severity::Error, diagnostics::err::InvalidEscape,
                             std::string("Invalid escape sequence '\\") + esc + "'",
                             spanAt(now - 1));
            }
            now++;
        } else {
            now++;
        }
    }

    diags.report(diagnostics::Severity::Error, diagnostics::err::UnclosedString,
                 "Unterminated string literal", spanRange(before, now));
}

void Lexer::processIdentifier() {
    const auto before = now;
    while (now < end && (isAsciiAlnum(*now) || *now == '_')) {
        now++;
    }
    std::string_view word{before, static_cast<size_t>(now - before)};
    TokenKind kind = lookupKeyword(word);
    tokens.emplace(Token{spanRange(before, now), kind});
}

Lexer::Lexer(memory::SourceMap &source_map, memory::Arena &arena,
             diagnostics::DiagnosticEngine &diags_)
    : sourceMap(source_map), diags(diags_), tokens(arena) {}

auto Lexer::run(std::variant<memory::FileId, std::pair<std::string_view, std::string>> input)
    -> memory::Result<TokenStream> {

    if (auto *id = std::get_if<memory::FileId>(&input)) {
        gId = *id;
    } else {
        auto &[name, content] = std::get<std::pair<std::string_view, std::string>>(input);
        auto add_result       = sourceMap.addFile(name, content);
        if (!add_result)
            return memory::Error{add_result.error().msg};
        gId = add_result.value();
    }

    if (auto i = sourceMap.get(gId)) {
        file = &i.value().get();
    }

    if (!file)
        return memory::Error{"[error] Couldnt find the file"};

    const auto content = file->getSlice();
    start = now = content.data();
    end         = content.data() + content.size();

    while (isOpen()) {
        if (isAsciiSpace(*now)) {
            now++;
            continue;
        }

        if (match("///")) {
            singleLineComment(true);
            continue;
        }

        if (match("//")) {
            singleLineComment();
            continue;
        }

        if (match("/**")) {
            multiLineComment(true);
            continue;
        }

        if (match("/*")) {
            multiLineComment();
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

        if (isAsciiAlpha(*now) || *now == '_') {
            processIdentifier();
            continue;
        }

        if (*now == '@') {
            const auto before = now;
            now++;
            tokens.emplace(Token{spanRange(before, now), Annotation{}});
            continue;
        }

        if (isOperator(*now)) {
            const auto before = now;
            now++;
            tokens.emplace(Token{spanRange(before, now), Operator{*before}});
            continue;
        }

        if (isPunctuation(*now)) {
            const auto before = now;
            now++;
            tokens.emplace(Token{spanRange(before, now), Punc{*before}});
            continue;
        }

        {
            const auto before = now;
            now++;
            auto span = spanRange(before, now);
            diags.report(diagnostics::Severity::Error, diagnostics::err::UnknownToken,
                         std::string("Unexpected character '") + *before + "'", span);
            tokens.emplace(Token{span, Unknown{}});
        }
    }

    tokens.emplace(Token{spanRange(now, now), End{}});

    if (diags.hasErrors())
        return memory::Error{"tokenization failed \u2014 see diagnostics for details"};

    return TokenStream{tokens.getData(), static_cast<uint32_t>(tokens.size()), 0, start};
}

auto tokenize(memory::SourceMap &source_map, memory::Arena &arena, memory::FileId id,
              diagnostics::DiagnosticEngine &diags) -> memory::Result<TokenStream> {
    Lexer lexer(source_map, arena, diags);
    return lexer.run(std::variant<memory::FileId, std::pair<std::string_view, std::string>>(id));
}

auto tokenize(memory::SourceMap &source_map, memory::Arena &arena, std::string_view name,
              std::string content, diagnostics::DiagnosticEngine &diags)
    -> memory::Result<TokenStream> {
    Lexer lexer(source_map, arena, diags);
    return lexer.run(std::make_pair(name, std::move(content)));
}

static const char *declSubName(DeclSub s) {
    switch (s) {
    case DeclSub::Let:      return "Let";
    case DeclSub::Var:      return "Var";
    case DeclSub::Global:   return "Global";
    case DeclSub::Const:    return "Const";
    case DeclSub::Auto:     return "Auto";
    case DeclSub::Struct:   return "Struct";
    case DeclSub::Enum:     return "Enum";
    case DeclSub::Union:    return "Union";
    case DeclSub::Component: return "Component";
    case DeclSub::Typedef:  return "Typedef";
    case DeclSub::Macro:    return "Macro";
    case DeclSub::Word:     return "Word";
    }
    return "Declaration";
}

static const char *kwSubName(const KwSub &sub) {
    return std::visit([](auto &&arg) -> const char * {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, KwAs>)         return "As";
        else if constexpr (std::is_same_v<T, KwUsing>)  return "Using";
        else if constexpr (std::is_same_v<T, KwRaw>)    return "Raw";
        else if constexpr (std::is_same_v<T, KwMust>)   return "Must";
        else if constexpr (std::is_same_v<T, KwMutable>) return "Mutable";
        else if constexpr (std::is_same_v<T, KwIf>)     return "If";
        else if constexpr (std::is_same_v<T, KwFor>)    return "For";
        else if constexpr (std::is_same_v<T, KwIn>)     return "In";
        else if constexpr (std::is_same_v<T, KwWhen>)   return "When";
        else if constexpr (std::is_same_v<T, KwControl>) return "Control";
        else if constexpr (std::is_same_v<T, KwRequire>) return "Require";
        else if constexpr (std::is_same_v<T, KwIs>)     return "Is";
        else if constexpr (std::is_same_v<T, KwWord>)   return "Word";
        else if constexpr (std::is_same_v<T, KwLogical>) return "Logical";
        else if constexpr (std::is_same_v<T, KwModule>) return "Module";
        else if constexpr (std::is_same_v<T, KwImport>) return "Import";
        else if constexpr (std::is_same_v<T, KwExport>) return "Export";
        else if constexpr (std::is_same_v<T, KwFrom>)   return "From";
        else if constexpr (std::is_same_v<T, KwAlias>)  return "Alias";
        else if constexpr (std::is_same_v<T, KwType>)   return "Type";
        else if constexpr (std::is_same_v<T, KwSpawn>)  return "Spawn";
        else if constexpr (std::is_same_v<T, KwAwait>)  return "Await";
        else if constexpr (std::is_same_v<T, KwThrow>)  return "Throw";
        else if constexpr (std::is_same_v<T, KwVisibility>) return "Visibility";
        else if constexpr (std::is_same_v<T, Ownership>)  return "Ownership";
        else return "Keyword";
    }, sub);
}

static const char *litSubName(LitSub s) {
    switch (s) {
    case LitSub::Null:    return "LitVal";
    case LitSub::Bool:    return "LitVal";
    case LitSub::Decimal: return "LitVal";
    case LitSub::Hex:     return "LitVal";
    case LitSub::Oct:     return "LitVal";
    case LitSub::Bin:     return "LitVal";
    case LitSub::Float:   return "LitVal";
    case LitSub::String:  return "LitVal";
    case LitSub::Char:    return "LitVal";
    }
    return "LitVal";
}

static const char *fnSubName(FnSub s) {
    switch (s) {
    case FnSub::Default: return "Fn";
    case FnSub::Raw:     return "Fn";
    case FnSub::Const:   return "Fn";
    case FnSub::Extern:  return "Fn";
    case FnSub::Flow:    return "Fn";
    }
    return "Fn";
}

static const char *ifaceSubName(IfaceSub s) {
    switch (s) {
    case IfaceSub::Trait:     return "Trait";
    case IfaceSub::Interface: return "Interface";
    case IfaceSub::Implement: return "Implement";
    case IfaceSub::Extends:   return "Extends";
    case IfaceSub::Dyn:       return "Dyn";
    }
    return "Interface";
}

static const char *blkSubName(BlkSub s) {
    switch (s) {
    case BlkSub::Marker:  return "Marker";
    case BlkSub::Drop:    return "Drop";
    case BlkSub::Dock:    return "Dock";
    case BlkSub::Fail:   return "Fail";
    case BlkSub::With:   return "With";
    case BlkSub::Catch:  return "Catch";
    case BlkSub::Context: return "Context";
    case BlkSub::Use:    return "Use";
    }
    return "Block";
}

const char *tokenKindName(TokenKind k) noexcept {
    return std::visit([](auto &&arg) -> const char * {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Identifier>)  return "Identifier";
        else if constexpr (std::is_same_v<T, Declaration>) return declSubName(arg.sub);
        else if constexpr (std::is_same_v<T, Keyword>)     return kwSubName(arg.sub);
        else if constexpr (std::is_same_v<T, Literal>)     return litSubName(arg.sub);
        else if constexpr (std::is_same_v<T, Punc>)        return "Punctuation";
        else if constexpr (std::is_same_v<T, Operator>)    return "Operators";
        else if constexpr (std::is_same_v<T, Fn>)          return fnSubName(arg.sub);
        else if constexpr (std::is_same_v<T, Interface>)   return ifaceSubName(arg.sub);
        else if constexpr (std::is_same_v<T, Block>)       return blkSubName(arg.sub);
        else if constexpr (std::is_same_v<T, Comments>)    return "Comments";
        else if constexpr (std::is_same_v<T, Annotation>)  return "Annotation";
        else if constexpr (std::is_same_v<T, Unknown>)     return "Unknown";
        else if constexpr (std::is_same_v<T, End>)         return "End";
        else return "???";
    }, k);
}

void printTokens(const TokenStream &stream) noexcept {
    for (uint32_t i = 0; i < stream.len; ++i) {
        const auto &tok = stream.src[i];
        auto lexeme     = stream.lexeme(tok);
        printf("  %-16s \"%.*s\"  [%u..%u]\n", tokenKindName(tok.kind), (int)lexeme.size(),
               lexeme.data(), tok.span.start, tok.span.end);
    }
}

} // namespace zith::lexer
