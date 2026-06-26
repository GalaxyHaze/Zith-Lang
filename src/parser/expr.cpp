#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "parser/operators.hpp"
#include "parser/scan-helpers.hpp"

#include <cstdlib>
#include <string>

namespace zith::parser {

using lexer::TokenKind;

// ── expression parsing ─────────────────────────────────────────────────

ast::ExprId Parser::parsePrimary() {
    skipComments(*tok);

    // block: { stmt; stmt; trailing_expr? }
    if (consume('{'))
        return parseBlock();

    // if expression
    auto if_span = peek().span;
    if (match("if")) {
        auto cond               = parseExpr();
        ast::ExprId then_branch = ast::kInvalidExpr;
        if (check('{')) {
            advance();
            then_branch = parseBlock();
        }
        ast::ExprId else_branch = ast::kInvalidExpr;
        if (match("else"))
            else_branch = parseExpr();
        return bld->ifExpr(cond, then_branch, else_branch, spanFrom(if_span));
    }

    // while expression
    auto while_span = peek().span;
    if (match("while")) {
        consume('(');
        auto cond = parseExpr();
        consume(')');
        ast::ExprId body = ast::kInvalidExpr;
        if (check('{')) {
            advance();
            body = parseBlock();
        }
        return bld->whileExpr(cond, body, spanFrom(while_span));
    }

    // 'for' expression — desugar into while
    if (match("for")) {
        auto desugar_block = bld->block(memory::DynArray<ast::StmtId>{bld->arena()});
        auto &block        = std::get<ast::BlockNode>(bld->getExpr(desugar_block));

        if (check('(')) {
            advance();
            // init statement
            if (check(TokenKind::Variable)) {
                advance();
                if (check(TokenKind::Identifier)) {
                    auto name = lexeme();
                    advance();
                    ast::ExprId init_val = ast::kInvalidExpr;
                    if (check('=')) {
                        advance();
                        init_val = parseExpr();
                    }
                    block.stmts.push(bld->letStmt(name, false, init_val));
                }
            } else if (!check(';')) {
                block.stmts.push(bld->addStmt(parseExpr()));
            }
            consume(';');

            // condition (default true)
            ast::ExprId cond = ast::kInvalidExpr;
            if (!check(';')) {
                cond = parseExpr();
            }
            consume(';');

            // step expression
            ast::ExprId step = ast::kInvalidExpr;
            if (!check(')')) {
                step = parseExpr();
            }
            consume(')');

            // body
            ast::ExprId loop_body = ast::kInvalidExpr;
            if (check('{')) {
                advance();
                loop_body = parseBlock();
            }

            auto body_block = bld->block(memory::DynArray<ast::StmtId>{bld->arena()});
            auto &bb        = std::get<ast::BlockNode>(bld->getExpr(body_block));
            if (loop_body != ast::kInvalidExpr) {
                auto &lb = std::get<ast::BlockNode>(bld->getExpr(loop_body));
                for (auto s : lb.stmts)
                    bb.stmts.push(s);
                bb.trailing = lb.trailing;
            }
            if (step != ast::kInvalidExpr)
                bb.stmts.push(bld->addStmt(step));

            ast::ExprId while_cond =
                cond != ast::kInvalidExpr ? cond : bld->litExpr(ast::LitKind::Bool, "true");
            auto while_expr = bld->whileExpr(while_cond, body_block);
            block.stmts.push(bld->addStmt(while_expr));
        }

        return desugar_block;
    }

    // parens
    if (consume('(')) {
        auto expr = parseExpr();
        consume(')');
        return expr;
    }

    // literals
    if (check(TokenKind::LitVal)) {
        auto lit_span = peek().span;
        auto lit      = lexeme();
        advance();
        auto kind = ast::LitKind::Int;
        if (lit == "true" || lit == "false")
            kind = ast::LitKind::Bool;
        else if (lit == "null")
            kind = ast::LitKind::Nil;
        return bld->litExpr(kind, lit, lit_span);
    }

    // identifiers
    if (check(TokenKind::Identifier)) {
        auto name = lexeme();
        auto span = peek().span;
        advance();
        return bld->ident(name, span);
    }

    errorExpected("expression");
    return bld->inferExpr();
}

ast::ExprId Parser::parsePrefix() {
    skipComments(*tok);

    if (check('-')) {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Neg, operand, spanFrom(op_span));
    }
    if (check('!')) {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Not, operand, spanFrom(op_span));
    }
    auto not_span = peek().span;
    if (match("not")) {
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Not, operand, spanFrom(not_span));
    }
    if (check('&')) {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Ref, operand, spanFrom(op_span));
    }
    if (check('*')) {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Deref, operand, spanFrom(op_span));
    }

    return parsePrimary();
}

ast::ExprId Parser::parseExpr(int min_prec) {
    skipComments(*tok);
    auto lhs = parsePrefix();

    while (true) {
        skipComments(*tok);
        if (eof())
            break;

        if (lhs == ast::kInvalidExpr)
            break;

        auto &cur     = peek();
        auto lhs_span = bld->exprSpan(lhs);

        // ── Postfix: call ( args ) ──────────────────────────────
        if (cur.is(lexer::TokenKind::Punctuation) && cur.punc == '(') {
            advance();
            auto args = parseDelimited(*tok, bld->arena(), ')', [this] { return parseExpr(); });
            consume(')');
            lhs = bld->call(lhs, std::move(args),
                            memory::Span{lhs_span.file, lhs_span.start, previous().span.end});
            continue;
        }

        // ── Postfix: .field ──────────────────────────────────────────
        if (cur.is(lexer::TokenKind::Punctuation) && cur.punc == '.') {
            advance();
            auto field_span = peek().span;
            auto field_name = lexeme();
            advance();
            lhs = bld->field(lhs, field_name,
                             memory::Span{lhs_span.file, lhs_span.start, field_span.end});
            continue;
        }

        // ── Postfix: [index] ────────────────────────────────────
        if (cur.is(lexer::TokenKind::Punctuation) && cur.punc == '[') {
            advance();
            auto index = parseExpr();
            if (index != ast::kInvalidExpr && !consume(']'))
                errorExpected("']'");
            lhs = bld->index(lhs, index,
                             memory::Span{lhs_span.file, lhs_span.start, previous().span.end});
            continue;
        }

        // ── Postfix: unwrap ! ───────────────────────────────────
        if (cur.is(lexer::TokenKind::Operators) && cur.punc == '!' &&
            !peek(1).is(lexer::TokenKind::Operators)) {
            advance();
            lhs = bld->unary(ast::UnaryOp::Deref, lhs,
                             memory::Span{lhs_span.file, lhs_span.start, previous().span.end});
            continue;
        }

        // ── Word infix: and / or / xor ──────────────────────────
        if (cur.is(lexer::TokenKind::Logical)) {
            auto word         = lexeme();
            uint8_t word_prec = operators::wordPrec(word);
            if (word_prec == 0 || word_prec < min_prec)
                break;
            advance();
            auto rhs = parseExpr(word_prec + 1);
            lhs      = bld->binary(lhs, operators::binaryOpForWord(word), rhs, spanFrom(lhs, rhs));
            continue;
        }

        // ── Compound operator: ==, !=, <=, >=, <<, >> ───
        ast::BinaryOp compound_op;
        if (operators::tryCompoundOp(cur, peek(1), compound_op)) {
            uint8_t prec = operators::infixPrec(cur);
            if (prec < min_prec)
                break;
            advance(2);
            auto rhs = parseExpr(prec + 1);
            lhs      = bld->binary(lhs, compound_op, rhs, spanFrom(lhs, rhs));
            continue;
        }

        // ── Infix binary operator (single char) ─────────────────
        uint8_t prec = operators::infixPrec(cur);
        if (prec == 0 || prec < min_prec)
            break;

        // Check that it's really an infix operator, not part of a compound
        ast::BinaryOp compound_check;
        if (operators::tryCompoundOp(cur, peek(1), compound_check))
            break;

        advance();
        auto rhs = parseExpr(prec + 1);
        lhs      = bld->binary(lhs, operators::binaryOpForChar(cur.punc), rhs, spanFrom(lhs, rhs));
    }

    return lhs;
}

ast::ExprId Parser::parseExpr() {
    return parseExpr(0);
}

ast::ExprId Parser::parseBlock() {
    auto start_span = peek().span;
    // '{' already consumed by advance() in caller
    memory::DynArray<ast::StmtId> stmts{bld->arena()};
    while (!eof() && !check('}')) {
        skipComments(*tok);
        if (check('}'))
            break;
        stmts.push(parseStmt());
    }
    if (check('}'))
        advance();
    return bld->block(std::move(stmts), ast::kInvalidExpr, spanFrom(start_span));
}

} // namespace zith::parser
