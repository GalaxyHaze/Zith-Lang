#include "parser.hpp"
#include "memory/source-map.hpp"
#include "lexer/lexer.hpp"
#include "diagnostics/error-codes.hpp"

#include <string_view>
#include <vector>

namespace zith::parser {
    
namespace {

    using lexer::TokenKind;
    using ast::kInvalidDecl;
    using ast::kInvalidExpr;
    using ast::kInvalidStmt;
    using diagnostics::Severity;
    using namespace zith::diagnostics::err;

} // anonymous namespace

    // ── token helpers ──────────────────────────────────────────────────────

    std::string_view Parser::lexeme() {
        auto res = memory::SourceMap::snippet(tok->peek().span);
        return res.isOk() ? res.value() : std::string_view{};
    }

    const lexer::Token &Parser::peek() { return tok->peek(); }
    const lexer::Token &Parser::peek(uint32_t n) { return tok->peek(n); }
    void Parser::advance() { tok->advance(); }
    void Parser::advance(uint32_t n) { tok->advance(n); }
    bool Parser::eof() { return peek().is_eof(); }

    bool Parser::match(TokenKind kind) {
        return tok->match(kind);
    }

    bool Parser::expect(TokenKind kind) {
        if (match(kind)) return true;
        diag->report(Severity::Error, ExpectedExpr,
                       std::string("expected ") + lexer::tokenKindName(kind) +
                       " but got " + lexer::tokenKindName(peek().kind),
                       peek().span);
        return false;
    }

    bool Parser::consume(char c) {
        auto l = lexeme();
        if (l.size() == 1 && l[0] == c) {
            advance();
            return true;
        }
        return false;
    }

    // ── expression parsing ─────────────────────────────────────────────────

    ast::ExprId Parser::parsePrimary() {
        if (consume('{'))
            return parseBlock();

        if (peek().is(TokenKind::LitVal)) {
            auto lit = lexeme();
            advance();
            return bld->litExpr(ast::LitKind::Int, lit);
        }

        if (peek().is(TokenKind::Identifier)) {
            auto name = lexeme();
            advance();
            return bld->ident(name);
        }

        if (peek().is(TokenKind::Punctuation) && lexeme() == "(") {
            advance();
            auto inner = parseExpr();
            if (!consume(')'))
                diag->report(Severity::Error, UnclosedParen,
                               "expected ')'", peek().span);
            return inner;
        }

        diag->report(Severity::Error, ExpectedExpr,
                       "expected expression", peek().span);
        advance();
        return kInvalidExpr;
    }

    ast::ExprId Parser::parseExpr(int min_prec) {
        auto lhs = parsePrimary();
        (void)min_prec;
        return lhs;
    }

    ast::ExprId Parser::parseExpr() {
        return parseExpr(0);
    }

    ast::ExprId Parser::parseBlock() {
        memory::DynArray<ast::StmtId> stmts{memory::SessionArena};

        while (!eof()) {
            if (peek().is(TokenKind::Punctuation) && lexeme() == "}")
                break;
            stmts.push(parseStmt());
        }

        consume('}');
        return bld->block(std::move(stmts));
    }

    // ── statement parsing ──────────────────────────────────────────────────

    ast::StmtId Parser::parseStmt() {
        if (peek().is(TokenKind::Control) && lexeme() == "return") {
            advance();
            auto val = eof() || (peek().is(TokenKind::Punctuation) && lexeme() == "}")
                           ? kInvalidExpr
                           : parseExpr();
            return bld->retStmt(val);
        }

        if (peek().is(TokenKind::Variable)) {
            advance();
            if (!peek().is(TokenKind::Identifier)) {
                diag->report(Severity::Error, ExpectedIdent,
                               "expected variable name", peek().span);
                advance();
                return kInvalidStmt;
            }
            auto name = lexeme();
            advance();
            ast::ExprId init = kInvalidExpr;
            if (consume('='))
                init = parseExpr();
            return bld->letStmt(name, false, init);
        }

        auto expr = parseExpr();
        return bld->addStmt(expr);
    }

    // ── declaration parsing ────────────────────────────────────────────────

    ast::DeclId Parser::parseFnDecl() {
        if (!peek().is(TokenKind::Identifier)) {
            diag->report(Severity::Error, ExpectedIdent,
                           "expected function name", peek().span);
            return kInvalidDecl;
        }
        auto name = lexeme();
        advance();

        if (!consume('(')) {
            diag->report(Severity::Error, ExpectedExpr,
                           "expected '(' after function name", peek().span);
            return kInvalidDecl;
        }

        memory::DynArray<std::string_view> params{memory::SessionArena};
        while (!eof() && !(peek().is(TokenKind::Punctuation) && lexeme() == ")")) {
            if (!peek().is(TokenKind::Identifier)) {
                diag->report(Severity::Error, ExpectedIdent,
                               "expected parameter name", peek().span);
                advance();
                continue;
            }
            params.push(lexeme());
            advance();
            if (lexeme() == ",")
                advance();
        }
        consume(')');

        ast::ExprId body = kInvalidExpr;
        if (peek().is(TokenKind::Punctuation) && lexeme() == "{")
            body = parseBlock();

        return bld->fnDecl(name, std::move(params), body);
    }

    ast::DeclId Parser::parseDecl() {
        if (match(TokenKind::Fn))
            return parseFnDecl();

        diag->report(Severity::Error, ExpectedExpr,
                       "expected declaration", peek().span);
        advance();
        return kInvalidDecl;
    }

    ast::DeclId Parser::parseTopLevel() {
        ast::DeclId first = kInvalidDecl;
        while (!eof()) {
            auto id = parseDecl();
            if (first == kInvalidDecl)
                first = id;
        }
        return first;
    }

    // ── public API ─────────────────────────────────────────────────────────

    ProgramResult parseProgram(lexer::TokenStream tokens,
                                ast::AstBuilder &builder,
                                diagnostics::DiagnosticEngine &diags) {
        Parser p{&tokens, &builder, &diags};
        return p.parseTopLevel();
    }

} // namespace zith::parser
