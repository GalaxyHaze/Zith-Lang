#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "frontend/ast/ast-builder.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/parser/parse-result.hpp"
#include "frontend/parser/recovery.hpp"
#include <cstdint>

namespace zith::frontend::parser {

    class Parser {
        lexer::TokenStream tokens_;
        ast::AstBuilder& builder_;
        diagnostics::engine::DiagnosticEngine& diags_;

    public:
        Parser(lexer::TokenStream tokens, ast::AstBuilder& builder,
               diagnostics::engine::DiagnosticEngine& diags);

        ProgramResult parseProgram();

    private:
        [[nodiscard]] const lexer::Token& peek() const;
        [[nodiscard]] const lexer::Token& peek(uint32_t n) const;
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

}
