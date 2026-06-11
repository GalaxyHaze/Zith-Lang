#pragma once

#include "ast/ast-builder.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "import/symbol-table.hpp"
#include "lexer/token.hpp"
#include "parser/parse-result.hpp"
#include "parser/scan-result.hpp"

namespace zith::parser {

struct Parser {
    lexer::TokenStream *tok;
    ast::AstBuilder *bld;
    diagnostics::DiagnosticEngine *diag;
    ast::ProgramNode program;

    Parser(lexer::TokenStream *tok, ast::AstBuilder *bld, diagnostics::DiagnosticEngine *diag)
        : tok(tok), bld(bld), diag(diag), program(bld->arena()) {}

    std::string_view lexeme();
    const lexer::Token &peek();
    const lexer::Token &peek(uint32_t n);
    void advance();
    void advance(uint32_t n);
    bool eof();
    bool match(lexer::TokenKind kind);
    bool expect(lexer::TokenKind kind);
    bool consume(char c);

    ast::ExprId parsePrimary();
    ast::ExprId parseExpr(int min_prec);
    ast::ExprId parseExpr();
    ast::ExprId parseBlock();
    ast::StmtId parseStmt();
    ast::DeclId parseFnDecl();
    ast::DeclId parseDecl();

    void expandBodies(ScanResult &result);
};

ProgramResult parseProgram(lexer::TokenStream tokens, ast::AstBuilder &builder,
                           diagnostics::DiagnosticEngine &diags);

ScanResult scan(Parser &parser, import::SymbolTable &syms, bool sema = true);

} // namespace zith::parser
