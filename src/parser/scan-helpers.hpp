#pragma once

#include "ast/ast-ids.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-table.hpp"
#include "lexer/token.hpp"
#include "memory/span.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace zith::parser {

// Skip Comments and Docs tokens — used throughout all parser passes
inline void skipComments(lexer::TokenStream &tok) {
    while (!tok.is_empty()) {
        auto &t = tok.peek();
        if (t.is_not(lexer::TokenKind::Comments) && t.is_not(lexer::TokenKind::Docs))
            break;
        tok.advance();
    }
}

namespace scan_detail {

inline memory::Span spanFromOffset(uint32_t start, uint32_t end) {
    return {0, start, end};
}

inline uint32_t skipBody(lexer::TokenStream &tok) {
    if (tok.is_empty())
        return tok.offset;
    uint32_t depth = 1;
    tok.advance();
    while (!tok.is_empty() && depth > 0) {
        if (tok.peek().kind == lexer::TokenKind::End)
            break;
        if (tok.peek().punc == '{')
            depth++;
        else if (tok.peek().punc == '}')
            depth--;
        tok.advance();
    }
    return tok.offset;
}

inline const char *symKindName(import::SymKind k) {
    switch (k) {
    case import::SymKind::Fn:        return "a fn";
    case import::SymKind::Struct:    return "a struct";
    case import::SymKind::Trait:     return "a trait";
    case import::SymKind::Interface: return "a interface";
    case import::SymKind::Enum:      return "an enum";
    case import::SymKind::Alias:     return "an alias";
    case import::SymKind::Variable:  return "a variable";
    case import::SymKind::Module:    return "a module";
    case import::SymKind::Component: return "a component";
    case import::SymKind::Union:     return "a union";
    }
    return "unknown";
}

// skip past a balanced pair: ( ... ), { ... }, [ ... ]
// cursor must be ON the opening delimiter; it is consumed first.
inline void skipBalanced(lexer::TokenStream &tok, char open, char close) {
    int depth = 1;
    tok.advance();
    while (!tok.is_empty()) {
        if (tok.peek().is_eof())
            break;
        if (tok.peek().punc == open)
            depth++;
        else if (tok.peek().punc == close)
            depth--;
        tok.advance();
        if (depth == 0)
            break;
    }
}

// skip past a type expression during scan — advances past tokens until a
// terminator (comma, brace, bracket, semicolon, equals, paren) is found.
inline void skipTypeExpr(lexer::TokenStream &tok) {
    while (!tok.is_empty()) {
        auto &t = tok.peek();
        if (t.is_eof())
            break;
        if (t.punc == ',' || t.punc == '}' || t.punc == ']' || t.punc == ';' || t.punc == '=' ||
            t.punc == ')' || t.punc == '|' || t.punc == '{')
            break;
        if (t.punc == '(') { skipBalanced(tok, '(', ')'); continue; }
        if (t.punc == '|') { skipBalanced(tok, '|', '|'); continue; }
        if (t.punc == '[') { skipBalanced(tok, '[', ']'); continue; }
        tok.advance();
    }
}

// skip past an expression during scan until a terminator
inline void skipExpr(lexer::TokenStream &tok) {
    while (!tok.is_empty()) {
        auto &t = tok.peek();
        if (t.is_eof())
            break;
        if (t.punc == ',' || t.punc == '}' || t.punc == ';' || t.punc == ')')
            break;
        if (t.punc == '(') { skipBalanced(tok, '(', ')'); continue; }
        if (t.punc == '{') { skipBalanced(tok, '{', '}'); continue; }
        if (t.punc == '[') { skipBalanced(tok, '[', ']'); continue; }
        tok.advance();
    }
}

inline bool reportIfDuplicate(import::SymbolTable &syms, diagnostics::DiagnosticEngine &diag,
                              std::string_view name, memory::Span span,
                              import::SymId skip_id = import::kInvalidSym) {
    auto existing = syms.lookup(name);
    if (existing == import::kInvalidSym || existing == skip_id)
        return false;
    auto &prev      = syms.get(existing);
    std::string msg = "duplicate symbol '";
    msg += name;
    msg += "' — previous declaration is ";
    msg += symKindName(prev.kind);
    diag.report(diagnostics::Severity::Error, diagnostics::err::DuplicateDecl, std::move(msg), span);
    return true;
}

} // namespace scan_detail
} // namespace zith::parser
