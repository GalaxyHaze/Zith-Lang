#pragma once

#include "ast/ast-nodes.hpp"
#include "lexer/token.hpp"

namespace zith::parser::operators {

inline uint8_t infixPrec(const lexer::Token &t) {
    if (t.is(lexer::TokenKind::Punctuation)) {
        switch (t.punc) {
        case '|':
            return 6;
        case '^':
            return 7;
        case '&':
            return 8;
        case '<':
            return 10;
        case '>':
            return 10;
        case '+':
            return 12;
        case '-':
            return 12;
        case '*':
            return 14;
        case '/':
            return 14;
        case '%':
            return 14;
        default:
            return 0;
        }
    }
    if (t.is(lexer::TokenKind::Logical))
        return 2;
    if (t.is(lexer::TokenKind::Operators)) {
        switch (t.punc) {
        case '|':
            return 6;
        case '^':
            return 7;
        case '&':
            return 8;
        case '<':
            return 10;
        case '>':
            return 10;
        case '+':
            return 12;
        case '-':
            return 12;
        case '*':
            return 14;
        case '/':
            return 14;
        case '%':
            return 14;
        case '=':
            return 0;
        case '!':
            return 0;
        case '~':
            return 0;
        case '?':
            return 0;
        default:
            return 0;
        }
    }
    return 0;
}

inline ast::BinaryOp binaryOpForWord(std::string_view w) {
    if (w == "and")
        return ast::BinaryOp::And;
    if (w == "or")
        return ast::BinaryOp::Or;
    if (w == "xor")
        return ast::BinaryOp::Xor;
    return ast::BinaryOp::Add;
}

inline uint8_t wordPrec(std::string_view w) {
    if (w == "or")
        return 2;
    if (w == "xor")
        return 4;
    if (w == "and")
        return 3;
    return 0;
}

inline bool tryCompoundOp(const lexer::Token &first, const lexer::Token &second,
                          ast::BinaryOp &out_op) {
    if (!first.is(lexer::TokenKind::Operators) || !second.is(lexer::TokenKind::Operators))
        return false;
    if (first.punc == '=' && second.punc == '=') {
        out_op = ast::BinaryOp::Eq;
        return true;
    }
    if (first.punc == '!' && second.punc == '=') {
        out_op = ast::BinaryOp::Ne;
        return true;
    }
    if (first.punc == '<' && second.punc == '=') {
        out_op = ast::BinaryOp::Le;
        return true;
    }
    if (first.punc == '>' && second.punc == '=') {
        out_op = ast::BinaryOp::Ge;
        return true;
    }
    if (first.punc == '<' && second.punc == '<') {
        out_op = ast::BinaryOp::Shl;
        return true;
    }
    if (first.punc == '>' && second.punc == '>') {
        out_op = ast::BinaryOp::Shr;
        return true;
    }
    if (first.punc == '=' && second.punc == '>') {
        out_op = ast::BinaryOp::Eq;
        return true;
    }
    return false;
}

inline ast::BinaryOp binaryOpForChar(char c) {
    switch (c) {
    case '+':
        return ast::BinaryOp::Add;
    case '-':
        return ast::BinaryOp::Sub;
    case '*':
        return ast::BinaryOp::Mul;
    case '/':
        return ast::BinaryOp::Div;
    case '%':
        return ast::BinaryOp::Rest;
    case '<':
        return ast::BinaryOp::Lt;
    case '>':
        return ast::BinaryOp::Gt;
    case '=':
        return ast::BinaryOp::Eq;
    case '!':
        return ast::BinaryOp::Ne;
    case '&':
        return ast::BinaryOp::And;
    case '|':
        return ast::BinaryOp::Or;
    default:
        return ast::BinaryOp::Add;
    }
}

} // namespace zith::parser::operators
