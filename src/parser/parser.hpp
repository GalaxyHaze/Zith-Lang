#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-builder.hpp"
#include "lexer/token.hpp"
#include "parser/parse-result.hpp"
#include "parser/recovery.hpp"


namespace zith::parser {

    struct Parser {
        lexer::TokenStream *tok;
        ast::AstBuilder *bld;
        diagnostics::DiagnosticEngine *diag;

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
        ast::DeclId parseTopLevel();
    };

    ProgramResult parseProgram(lexer::TokenStream tokens,
                                ast::AstBuilder &builder,
                                diagnostics::DiagnosticEngine &diags);

} // namespace zith::parser
