#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-builder.hpp"
#include "lexer/token.hpp"
#include "parser/parse-result.hpp"
#include "parser/recovery.hpp"

#include <cstdint>

namespace zith::parser {

    class Parser {
        lexer::TokenStream tokens_;
        ast::AstBuilder &builder_;
        diagnostics::DiagnosticEngine &diags_;

    public:
        Parser(lexer::TokenStream tokens,
               ast::AstBuilder &builder,
               diagnostics::DiagnosticEngine &diags);

        ProgramResult parseProgram();

    private:
        [[nodiscard]] const lexer::Token &peek() const;
        [[nodiscard]] const lexer::Token &peek(uint32_t n) const;
        void advance();
        void advance(uint32_t n);
        bool match(lexer::TokenKind kind);
        bool expect(lexer::TokenKind kind);

        ast::ExprId parseExpr();
        ast::ExprId parseExpr(int min_prec);
        ast::ExprId parsePrimary();
        ast::ExprId parseBlock();

        ast::StmtId parseStmt();
        ast::StmtId parseLetStmt();
        ast::StmtId parseRetStmt();

        ast::DeclId parseDecl();
        ast::DeclId parseFnDecl();
        ast::DeclId parseStructDecl();

        int precedence(lexer::TokenKind kind) const;
    };

} // namespace zith::parser
