#pragma once

#include "ast/ast-builder.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "import/symbol-table.hpp"
#include "lexer/token.hpp"
#include "parser/parse-result.hpp"
#include "parser/scan-result.hpp"

#include <initializer_list>
#include <string_view>

namespace zith::parser {

struct Parser {
    lexer::TokenStream *tok;
    ast::AstBuilder *bld;
    diagnostics::DiagnosticEngine *diag;
    ast::ProgramNode program;

    Parser(lexer::TokenStream *tok, ast::AstBuilder *bld, diagnostics::DiagnosticEngine *diag)
        : tok(tok), bld(bld), diag(diag), program(bld->arena()) {}

    // ── Token helpers ────────────────────────────────────────────────
    std::string_view lexeme();
    const lexer::Token &peek();
    const lexer::Token &peek(uint32_t n);
    void advance();
    void advance(uint32_t n);
    [[nodiscard]] bool eof();
    bool match(lexer::TokenKind kind);
    bool expect(lexer::TokenKind kind);
    bool consume(char c);
    bool consume(lexer::TokenKind kind);
    bool expectPunc(char c);
    bool expectIdent(std::string_view &out);
    std::string_view expectIdent();

    // ── Expression parsing ───────────────────────────────────────────
    ast::ExprId parsePrimary();
    ast::ExprId parsePrefix();
    ast::ExprId parseExpr(int min_prec);
    ast::ExprId parseExpr();
    ast::ExprId parseBlock();
    ast::StmtId parseStmt();
    ast::DeclId parseFnDecl();
    ast::DeclId parseDecl();

    // ── Type expression parsing ─────────────────────────────────────
    ast::TypeExprId parseTypeExpr();
    ast::TypeExprId parseOrExpr();
    ast::TypeExprId parsePrimaryType();

    // ── Helper patterns ──────────────────────────────────────────────

    // Parse `: Type` annotation if present, returns kInvalidTypeExpr otherwise
    ast::TypeExprId parseOptTypeAnnotation();

    // Parse comma-separated list delimited by `close` (e.g., `)` for parens)
    template <typename Fn>
    auto parseCommaList(char close, Fn parser) -> memory::DynArray<decltype(parser())> {
        using T = decltype(parser());
        auto result = memory::DynArray<T>{bld->arena()};
        if (peek().punc != close) {
            result.push(parser());
            while (consume(','))
                result.push(parser());
        }
        return result;
    }

    // Skip tokens until a sync point is found (error recovery)
    void skipUntil(std::initializer_list<lexer::TokenKind> sync_tokens);

    // ── Body expansion ───────────────────────────────────────────────
    void expandBodies(ScanResult &result);

    memory::Span spanFrom(memory::Span start) const;
    memory::Span spanFrom(ast::ExprId lhs, ast::ExprId rhs) const;
};

ProgramResult parseProgram(lexer::TokenStream tokens, ast::AstBuilder &builder,
                           diagnostics::DiagnosticEngine &diags);

ScanResult scan(Parser &parser, import::SymbolTable &syms);

} // namespace zith::parser
