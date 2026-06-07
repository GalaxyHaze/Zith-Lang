#include "parser.hpp"
#include "parser/source-map.hpp"
#include "lexer/lexer.hpp"
#include "diagnostics/error-codes.hpp"

#include <string_view>
#include <vector>

namespace zith::parser {
namespace {

    // ── module-level state ─────────────────────────────────────────────────
    lexer::TokenStream *s_tok = nullptr;
    ast::AstBuilder *s_bld = nullptr;
    diagnostics::DiagnosticEngine *s_diag = nullptr;

    using lexer::TokenKind;
    using ast::kInvalidDecl;
    using ast::kInvalidExpr;
    using ast::kInvalidStmt;
    using diagnostics::Severity;
    using namespace zith::diagnostics::err;

    // ── token helpers ──────────────────────────────────────────────────────

    static std::string_view lexeme() {
        auto res = SourceMap::snippet(s_tok->peek().span);
        return res.isOk() ? res.value() : std::string_view{};
    }

    static const lexer::Token &peek() { return s_tok->peek(); }
    static const lexer::Token &peek(uint32_t n) { return s_tok->peek(n); }
    static void advance() { s_tok->advance(); }
    static void advance(uint32_t n) { s_tok->advance(n); }
    static bool eof() { return peek().is_eof(); }

    static bool match(TokenKind kind) {
        return s_tok->match(kind);
    }

    static bool expect(TokenKind kind) {
        if (match(kind)) return true;
        s_diag->report(Severity::Error, ExpectedExpr,
                       std::string("expected ") + lexer::tokenKindName(kind) +
                       " but got " + lexer::tokenKindName(peek().kind),
                       peek().span);
        return false;
    }

    static bool consume(char c) {
        auto l = lexeme();
        if (l.size() == 1 && l[0] == c) {
            advance();
            return true;
        }
        return false;
    }

    // ── forward declarations ───────────────────────────────────────────────
    static ast::ExprId parseExpr();
    static ast::ExprId parseBlock();
    static ast::StmtId parseStmt();
    static ast::DeclId parseDecl();
    static ast::DeclId parseFnDecl();

    // ── expression parsing ─────────────────────────────────────────────────

    static ast::ExprId parsePrimary() {
        if (consume('{'))
            return parseBlock();

        if (peek().is(TokenKind::LitVal)) {
            auto lit = lexeme();
            advance();
            return s_bld->litExpr(ast::LitKind::Int, lit);
        }

        if (peek().is(TokenKind::Identifier)) {
            auto name = lexeme();
            advance();
            return s_bld->ident(name);
        }

        if (peek().is(TokenKind::Punctuation) && lexeme() == "(") {
            advance();
            auto inner = parseExpr();
            if (!consume(')'))
                s_diag->report(Severity::Error, UnclosedParen,
                               "expected ')'", peek().span);
            return inner;
        }

        s_diag->report(Severity::Error, ExpectedExpr,
                       "expected expression", peek().span);
        advance();
        return kInvalidExpr;
    }

    static ast::ExprId parseExpr(int min_prec) {
        auto lhs = parsePrimary();
        (void)min_prec;
        return lhs;
    }

    static ast::ExprId parseExpr() {
        return parseExpr(0);
    }

    static ast::ExprId parseBlock() {
        std::vector<ast::StmtId> stmts;

        while (!eof()) {
            if (peek().is(TokenKind::Punctuation) && lexeme() == "}")
                break;
            stmts.push_back(parseStmt());
        }

        consume('}');
        return s_bld->block(std::move(stmts));
    }

    // ── statement parsing ──────────────────────────────────────────────────

    static ast::StmtId parseStmt() {
        if (peek().is(TokenKind::Control) && lexeme() == "return") {
            advance();
            auto val = eof() || (peek().is(TokenKind::Punctuation) && lexeme() == "}")
                           ? kInvalidExpr
                           : parseExpr();
            return s_bld->retStmt(val);
        }

        if (peek().is(TokenKind::Variable)) {
            advance();
            if (!peek().is(TokenKind::Identifier)) {
                s_diag->report(Severity::Error, ExpectedIdent,
                               "expected variable name", peek().span);
                advance();
                return kInvalidStmt;
            }
            auto name = lexeme();
            advance();
            ast::ExprId init = kInvalidExpr;
            if (consume('='))
                init = parseExpr();
            return s_bld->letStmt(name, false, init);
        }

        auto expr = parseExpr();
        return s_bld->addStmt(expr);
    }

    // ── declaration parsing ────────────────────────────────────────────────

    static ast::DeclId parseFnDecl() {
        if (!peek().is(TokenKind::Identifier)) {
            s_diag->report(Severity::Error, ExpectedIdent,
                           "expected function name", peek().span);
            return kInvalidDecl;
        }
        auto name = lexeme();
        advance();

        if (!consume('(')) {
            s_diag->report(Severity::Error, ExpectedExpr,
                           "expected '(' after function name", peek().span);
            return kInvalidDecl;
        }

        std::vector<std::string_view> params;
        while (!eof() && !(peek().is(TokenKind::Punctuation) && lexeme() == ")")) {
            if (!peek().is(TokenKind::Identifier)) {
                s_diag->report(Severity::Error, ExpectedIdent,
                               "expected parameter name", peek().span);
                advance();
                continue;
            }
            params.push_back(lexeme());
            advance();
            if (lexeme() == ",")
                advance();
        }
        consume(')');

        ast::ExprId body = kInvalidExpr;
        if (peek().is(TokenKind::Punctuation) && lexeme() == "{")
            body = parseBlock();

        return s_bld->fnDecl(name, std::move(params), body);
    }

    static ast::DeclId parseDecl() {
        if (match(TokenKind::Fn))
            return parseFnDecl();

        s_diag->report(Severity::Error, ExpectedExpr,
                       "expected declaration", peek().span);
        advance();
        return kInvalidDecl;
    }

    static ast::DeclId parseTopLevel() {
        ast::DeclId first = kInvalidDecl;
        while (!eof()) {
            auto id = parseDecl();
            if (first == kInvalidDecl)
                first = id;
        }
        return first;
    }

} // anonymous namespace

    // ── public API ─────────────────────────────────────────────────────────

    ProgramResult parseProgram(lexer::TokenStream tokens,
                                ast::AstBuilder &builder,
                                diagnostics::DiagnosticEngine &diags) {
        s_tok  = &tokens;
        s_bld  = &builder;
        s_diag = &diags;

        auto first = parseTopLevel();

        s_tok  = nullptr;
        s_bld  = nullptr;
        s_diag = nullptr;

        return {first, {}, true};


    }

} // namespace zith::parser
