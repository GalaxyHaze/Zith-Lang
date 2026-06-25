#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "lexer/lexer.hpp"
#include "parser/recovery.hpp"
#include "parser/scan-helpers.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace zith::parser {

using ast::kInvalidDecl;
using ast::kInvalidExpr;
using ast::kInvalidStmt;
using diagnostics::Severity;
using lexer::TokenKind;
using namespace diagnostics::err;

// ── token helpers ──────────────────────────────────────────────────────

std::string_view Parser::lexeme() {
    return tok->lexeme();
}

const lexer::Token &Parser::peek() {
    return tok->peek();
}

const lexer::Token &Parser::peek(uint32_t n) {
    return tok->peek(n);
}

void Parser::advance() {
    tok->advance();
}

void Parser::advance(uint32_t n) {
    tok->advance(n);
}

bool Parser::eof() {
    return tok->is_empty();
}

bool Parser::match(TokenKind kind) {
    if (!eof() && peek().is(kind)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::expect(TokenKind kind) {
    if (tok->peek().is(kind)) {
        advance();
        return true;
    }
    diag->report(Severity::Error, ExpectedExpr,
                 std::string("expected ") + lexer::tokenKindName(kind) + " but got " +
                     lexer::tokenKindName(peek().kind),
                 peek().span);
    return false;
}

bool Parser::consume(char c) {
    if (!eof() && peek().punc == c) {
        advance();
        return true;
    }
    return false;
}

bool Parser::consume(TokenKind kind) {
    if (!eof() && peek().is(kind)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::expectPunc(char c) {
    if (!eof() && peek().punc == c) {
        advance();
        return true;
    }
    std::string msg = "expected '";
    msg += c;
    msg += "'";
    diag->report(Severity::Error, ExpectedExpr, std::move(msg), peek().span);
    return false;
}

bool Parser::expectIdent(std::string_view &out) {
    if (!eof() && peek().is(TokenKind::Identifier)) {
        out = lexeme();
        advance();
        return true;
    }
    diag->report(Severity::Error, ExpectedIdent, "expected identifier", peek().span);
    return false;
}

std::string_view Parser::expectIdent() {
    if (!eof() && peek().is(TokenKind::Identifier)) {
        auto name = lexeme();
        advance();
        return name;
    }
    diag->report(Severity::Error, ExpectedIdent, "expected identifier", peek().span);
    return {};
}

memory::Span Parser::spanFrom(memory::Span start) const {
    if (tok->offset > 0) {
        auto &prev = tok->src[tok->offset - 1];
        return {start.file, start.start, prev.span.end};
    }
    return start;
}

memory::Span Parser::spanFrom(ast::ExprId lhs, ast::ExprId rhs) const {
    auto ls = bld->exprSpan(lhs);
    auto rs = (rhs != ast::kInvalidExpr) ? bld->exprSpan(rhs) : ls;
    return {ls.file, ls.start, rs.end};
}

// ── statement parsing ──────────────────────────────────────────────────

ast::StmtId Parser::parseStmt() {
    skipComments(*tok);
    if (peek().is(TokenKind::Control) && lexeme() == "return") {
        auto ret_span = peek().span;
        advance();
        auto val = eof() || peek().punc == '}' ? kInvalidExpr : parseExpr();
        if (peek().punc != ';')
            skipUntil({TokenKind::End, TokenKind::Punctuation});
        else
            advance();
        return bld->retStmt(val, spanFrom(ret_span));
    }

    if (peek().is(TokenKind::Control) && lexeme() == "break") {
        auto br_span = peek().span;
        advance();
        if (peek().punc == ';')
            advance();
        auto nil = bld->litExpr(ast::LitKind::Nil, "null", peek().span);
        return bld->addStmt(nil);
    }

    if (peek().is(TokenKind::Control) && lexeme() == "continue") {
        auto cont_span = peek().span;
        advance();
        if (peek().punc == ';')
            advance();
        auto nil = bld->litExpr(ast::LitKind::Nil, "null", peek().span);
        return bld->addStmt(nil);
    }

    if (peek().is(TokenKind::Variable)) {
        auto let_span = peek().span;
        advance();
        auto name = expectIdent();
        auto type_annot = parseOptTypeAnnotation();
        ast::ExprId init = kInvalidExpr;
        if (consume('='))
            init = parseExpr();
        if (peek().punc == ';')
            advance();
        memory::DynArray<std::string_view> names{bld->arena()};
        names.push(name);
        return bld->letStmt(std::move(names), false, type_annot, init, spanFrom(let_span));
    }

    auto expr = parseExpr();
    if (peek().punc == '=') {
        auto assign_span = peek().span;
        advance();
        auto rhs = parseExpr();
        if (peek().punc == ';')
            advance();
        return bld->assign(expr, rhs, spanFrom(assign_span));
    }
    if (peek().punc == ';')
        advance();
    return bld->addStmt(expr);
}

// ── declaration parsing ────────────────────────────────────────────────

ast::DeclId Parser::parseFnDecl() {
    if (!peek().is(TokenKind::Identifier)) {
        diag->report(Severity::Error, ExpectedIdent, "expected function name", peek().span);
        return kInvalidDecl;
    }
    auto name_span = peek().span;
    auto name      = lexeme();
    advance();

    if (!consume('(')) {
        diag->report(Severity::Error, ExpectedExpr, "expected '(' after function name",
                     peek().span);
        return kInvalidDecl;
    }

    memory::DynArray<std::string_view> params{bld->arena()};
    while (!eof() && peek().punc != ')') {
        if (!peek().is(TokenKind::Identifier)) {
            diag->report(Severity::Error, ExpectedIdent, "expected parameter name", peek().span);
            advance();
            continue;
        }
        params.push(lexeme());
        advance();
        if (peek().punc == ',')
            advance();
    }
    consume(')');

    ast::ExprId body = kInvalidExpr;
    if (peek().punc == '{')
        body = parseBlock();

    return bld->fnDecl(name, std::move(params), body, spanFrom(name_span));
}

ast::DeclId Parser::parseDecl() {
    skipComments(*tok);
    if (peek().is(TokenKind::Fn))
        return parseFnDecl();
    if (peek().is(TokenKind::Struct))
        return kInvalidDecl; // handled during scan
    diag->report(Severity::Error, ExpectedExpr, "expected declaration", peek().span);
    advance();
    return kInvalidDecl;
}

// ── Parser helper implementations ───────────────────────────────────────

ast::TypeExprId Parser::parseOptTypeAnnotation() {
    if (peek().punc == ':') {
        advance();
        return parseTypeExpr();
    }
    return ast::kInvalidTypeExpr;
}

void Parser::skipUntil(std::initializer_list<lexer::TokenKind> sync_tokens) {
    recovery::panic(*tok, sync_tokens);
}

// ── public API (backward compat) ─────────────────────────────────────

ProgramResult parseProgram(lexer::TokenStream tokens, ast::AstBuilder &builder,
                           diagnostics::DiagnosticEngine &diags) {
    memory::Arena arena;
    import::SymbolTable syms(arena);
    Parser p(&tokens, &builder, &diags);
    auto result = scan(p, syms);
    p.expandBodies(result);
    return std::move(p.program);
}

} // namespace zith::parser
