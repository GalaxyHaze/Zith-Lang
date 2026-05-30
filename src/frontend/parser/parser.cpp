#include "parser.hpp"

namespace zith::frontend::parser {

    Parser::Parser(lexer::TokenStream tokens, ast::AstBuilder& builder,
                   diagnostics::engine::DiagnosticEngine& diags)
        : tokens_(tokens), builder_(builder), diags_(diags) {}

    ProgramResult Parser::parseProgram() {
        return ProgramResult{ast::kInvalidDecl, {}, false};
    }

    const lexer::Token& Parser::peek() const { return tokens_.peek(); }
    const lexer::Token& Parser::peek(uint32_t n) const { return tokens_.peek(n); }
    void Parser::advance() { tokens_.advance(); }
    void Parser::advance(uint32_t n) { tokens_.advance(n); }
    bool Parser::match(lexer::TokenKind kind) { return tokens_.match(kind); }
    bool Parser::expect(lexer::TokenKind kind) { (void)kind; return false; }

    ast::ExprId Parser::parseExpr() { return ast::kInvalidExpr; }
    ast::ExprId Parser::parseExpr(int) { return ast::kInvalidExpr; }
    ast::ExprId Parser::parsePrimary() { return ast::kInvalidExpr; }
    ast::ExprId Parser::parseBlock() { return ast::kInvalidExpr; }
    ast::StmtId Parser::parseStmt() { return ast::kInvalidStmt; }
    ast::StmtId Parser::parseLetStmt() { return ast::kInvalidStmt; }
    ast::StmtId Parser::parseRetStmt() { return ast::kInvalidStmt; }
    ast::DeclId Parser::parseDecl() { return ast::kInvalidDecl; }
    ast::DeclId Parser::parseFnDecl() { return ast::kInvalidDecl; }
    ast::DeclId Parser::parseStructDecl() { return ast::kInvalidDecl; }
    int Parser::precedence(lexer::TokenKind) const { return 0; }

}
