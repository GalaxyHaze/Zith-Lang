#include "frontend/ast-lowerer.hpp"

#include <optional>
#include <string_view>

namespace zith::frontend {

bool AstLowerer::isOperatorToken(std::string_view op) const noexcept {
    return index_ < token_count_ && snapshot_.tokens_[index_].kind == TokenKind::Operator &&
           text(index_) == op;
}

/// True when the current `<` opens a generic application `name<A, B>(...)`
/// or a generic struct literal `name<A, B>{ ... }`: the matching `>`
/// (counting nested `<`/`>`) must be immediately followed by `(` or `{`.
/// Plain comparisons like `a < b` do not match because they are not followed
/// by a call or literal brace.
bool AstLowerer::isGenericApplication() const noexcept {
    if (!isOperatorToken("<"))
        return false;
    int depth = 0;
    for (uint32_t i = index_; i < token_count_; ++i) {
        const auto &token = snapshot_.tokens_[i];
        if (token.kind == TokenKind::Operator) {
            if (text(i) == "<")
                ++depth;
            else if (text(i) == ">") {
                --depth;
                if (depth == 0) {
                    if (i + 1U >= token_count_ ||
                        snapshot_.tokens_[i + 1U].kind != TokenKind::Punctuation)
                        return false;
                    const auto next = text(i + 1U);
                    return next == "(" || next == "{";
                }
            }
        } else if (token.kind == TokenKind::Punctuation && text(i) == "(") {
            // Nested calls/grouping inside the args would break the heuristic; only
            // accept angle brackets without stray parens.
            return false;
        } else if (token.kind == TokenKind::End) {
            return false;
        }
    }
    return false;
}

bool AstLowerer::isKeywordToken(std::string_view word) const noexcept {
    return index_ < token_count_ && snapshot_.tokens_[index_].kind == TokenKind::Keyword &&
           text(index_) == word;
}

bool AstLowerer::isVisibilityPrefix() const noexcept {
    if (index_ >= token_count_)
        return false;
    const auto word = text(index_);
    return word == "pub" || word == "mod";
}

/// Parses a valid function-kind prefix for this lowerer's current position:
/// `fn`, `const fn`, `raw fn`, `extern fn`, or `flow fn`.  Returns false when
/// the current token is not `fn` or a kind prefix followed by `fn`.
std::optional<FunctionKind> AstLowerer::functionKindPrefix() {
    return ::zith::frontend::functionKindPrefix(snapshot_, index_, token_count_);
}

int AstLowerer::precedence(std::string_view op) noexcept {
    if (op == "|>")
        return 0;
    if (isAssignmentOp(op))
        return 1;
    if (op == "or")
        return 2;
    if (op == "and")
        return 3;
    if (op == "xor")
        return 4;
    if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=")
        return 5;
    if (op == "in")
        return 5;
    if (op == "|.")
        return 6;
    if (op == "^.")
        return 7;
    if (op == "&.")
        return 8;
    if (op == "<<" || op == ">>")
        return 9;
    if (op == "+" || op == "-")
        return 10;
    if (op == "*" || op == "/" || op == "%")
        return 11;
    return -1;
}

bool AstLowerer::isAssignmentOp(std::string_view op) noexcept {
    return op == "=" || compoundBaseOp(op) != std::string_view{};
}

bool AstLowerer::isRangeDotAt(uint32_t offset) const noexcept {
    const uint32_t i = index_ + offset;
    return i < token_count_ && snapshot_.tokens_[i].kind == TokenKind::Dots &&
           text(i) == "..";
}

bool AstLowerer::isRangeOpenAt(uint32_t offset) const noexcept {
    const uint32_t i = index_ + offset;
    return i + 1U < token_count_ && snapshot_.tokens_[i].kind == TokenKind::Operator &&
           text(i) == ">" && snapshot_.tokens_[i + 1U].kind == TokenKind::Dots &&
           text(i + 1U) == "..";
}

/// For a compound assignment, the base operator it desugars to. Empty for
/// anything else. The bitwise compounds drop the `.` of their base spelling
/// because there is no ambiguity in assignment position.
std::string_view AstLowerer::compoundBaseOp(std::string_view op) noexcept {
    if (op == "+=")
        return "+";
    if (op == "-=")
        return "-";
    if (op == "*=")
        return "*";
    if (op == "/=")
        return "/";
    if (op == "%=")
        return "%";
    if (op == "<<=")
        return "<<";
    if (op == ">>=")
        return ">>";
    if (op == "&=")
        return "&.";
    if (op == "|=")
        return "|.";
    if (op == "^=")
        return "^.";
    return {};
}

} // namespace zith::frontend
