#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "parser/operators.hpp"

#include <cstdlib>
#include <string>

namespace zith::parser {

using lexer::TokenKind;
using namespace operators;

namespace {

using ast::kInvalidDecl;
using ast::kInvalidExpr;
using ast::kInvalidStmt;
using diagnostics::Severity;

bool matchBuiltinType(std::string_view name, ast::BuiltinType &out) {
    if (name == "i8") {
        out = ast::BuiltinType::I8;
        return true;
    }
    if (name == "i16") {
        out = ast::BuiltinType::I16;
        return true;
    }
    if (name == "i32") {
        out = ast::BuiltinType::I32;
        return true;
    }
    if (name == "i64") {
        out = ast::BuiltinType::I64;
        return true;
    }
    if (name == "i128") {
        out = ast::BuiltinType::I128;
        return true;
    }
    if (name == "u8") {
        out = ast::BuiltinType::U8;
        return true;
    }
    if (name == "u16") {
        out = ast::BuiltinType::U16;
        return true;
    }
    if (name == "u32") {
        out = ast::BuiltinType::U32;
        return true;
    }
    if (name == "u64") {
        out = ast::BuiltinType::U64;
        return true;
    }
    if (name == "u128") {
        out = ast::BuiltinType::U128;
        return true;
    }
    if (name == "f32") {
        out = ast::BuiltinType::F32;
        return true;
    }
    if (name == "f64") {
        out = ast::BuiltinType::F64;
        return true;
    }
    if (name == "bool") {
        out = ast::BuiltinType::Bool;
        return true;
    }
    if (name == "char") {
        out = ast::BuiltinType::Char;
        return true;
    }
    if (name == "void") {
        out = ast::BuiltinType::Void;
        return true;
    }
    if (name == "never" || name == "noreturn") {
        out = ast::BuiltinType::Never;
        return true;
    }
    if (name == "invalid") {
        out = ast::BuiltinType::Invalid;
        return true;
    }
    if (name == "unknown") {
        out = ast::BuiltinType::Unknown;
        return true;
    }
    if (name == "opaque") {
        out = ast::BuiltinType::Opaque;
        return true;
    }
    return false;
}

} // anonymous namespace

ast::TypeExprId Parser::parsePrimaryType() {
    // ── builtin types (i32, bool, void, …) ──────────────────────────
    if (check(TokenKind::Type)) {
        ast::BuiltinType bt;
        if (matchBuiltinType(lexeme(), bt)) {
            advance();
            return bld->builtinExpr(bt);
        }
        auto segments = memory::DynArray<std::string_view>{bld->arena()};
        segments.push(lexeme());
        advance();
        return bld->pathExpr(std::move(segments));
    }

    // ── pointer / ownership-qualified types ─────────────────────────
    {
        bool has_mut               = false;
        ast::OwnershipKw ownership = ast::OwnershipKw::Default;

        if (consume(TokenKind::Mutable))
            has_mut = true;

        if (check(TokenKind::Ownership)) {
            auto kw = lexeme();
            if (kw == "unique")
                ownership = ast::OwnershipKw::Unique;
            if (kw == "share")
                ownership = ast::OwnershipKw::Share;
            if (kw == "lend")
                ownership = ast::OwnershipKw::Lend;
            if (kw == "view")
                ownership = ast::OwnershipKw::View;
            if (kw == "belong")
                ownership = ast::OwnershipKw::Belong;
            advance();

            if (!check('*')) {
                auto inner = parsePrimaryType();
                return bld->addTypeExpr(ast::TypePtrExpr{inner, has_mut, ownership});
            }
        }

        if (consume('*')) {
            auto inner = parsePrimaryType();
            return bld->addTypeExpr(ast::TypePtrExpr{inner, has_mut, ownership});
        }

        if (has_mut || ownership != ast::OwnershipKw::Default) {
            errorExpected("pointer type after 'mut'");
            return bld->inferExpr();
        }
    }

    // ── optional type: ?T ───────────────────────────────────────────
    if (consume('?')) {
        auto inner = parsePrimaryType();
        return bld->addTypeExpr(ast::TypeOptional{inner});
    }

    // ── pack type: |T, U| or |name: T, U| ─────────────────────────
    if (peek().punc == '|') {
        advance();
        memory::DynArray<ast::TypePackMember> members{bld->arena()};
        while (!peek().is_eof() && !check('|')) {
            if (check(TokenKind::Identifier)) {
                if (peek(1).punc == ':') {
                    auto name = lexeme();
                    advance();
                    advance();
                    members.push({name, parsePrimaryType()});
                } else {
                    members.push({std::string_view{}, parsePrimaryType()});
                }
            } else {
                members.push({std::string_view{}, parsePrimaryType()});
            }
            if (check(','))
                advance();
        }
        if (check('|'))
            advance();
        return bld->addTypeExpr(ast::TypePack{std::move(members)});
    }

    // ── parenthesized: (T) or fn(...) ─────────────────────────────
    if (consume('(')) {
        auto inner = parseTypeExpr();
        if (check(')'))
            advance();
        return inner;
    }

    // ── slice / array: []T or [N]T ────────────────────────────────
    if (consume('[')) {
        if (check(']')) {
            advance();
            return bld->addTypeExpr(ast::TypeSlice{parsePrimaryType()});
        }
        auto count_expr = bld->inferExpr();
        while (!peek().is_eof() && !check(']'))
            advance();
        if (check(']'))
            advance();
        return bld->addTypeExpr(ast::TypeArray{parsePrimaryType(), count_expr});
    }

    // ── fn type: fn(T): U ─────────────────────────────────────────
    if (consume(TokenKind::Fn)) {
        memory::DynArray<ast::TypeExprId> params{bld->arena()};
        if (consume('(')) {
            while (!peek().is_eof() && !check(')')) {
                params.push(parseTypeExpr());
                if (check(','))
                    advance();
            }
            if (check(')'))
                advance();
        }
        auto ret = bld->inferExpr();
        if (consume(':'))
            ret = parseTypeExpr();
        return bld->addTypeExpr(ast::TypeFnExpr{std::move(params), ret});
    }

    // ── infer type: _ ──────────────────────────────────────────────
    if (consume('_'))
        return bld->inferExpr();

    // ── identifier / path: Vec, std.vec.Vec ────────────────────────
    if (check(TokenKind::Identifier)) {
        auto id_span = peek().span;
        auto id      = lexeme();
        ast::BuiltinType bt;
        if (peek(1).punc != '.' && matchBuiltinType(id, bt)) {
            advance();
            return bld->builtinExpr(bt);
        }
        auto seg = id;
        advance();
        memory::DynArray<std::string_view> segments{bld->arena()};
        segments.push(seg);
        auto path_span = id_span;
        while (check('.')) {
            advance();
            if (check(TokenKind::Identifier)) {
                path_span.end = peek().span.end;
                segments.push(lexeme());
                advance();
            } else {
                break;
            }
        }
        return bld->pathExpr(std::move(segments), path_span);
    }

    errorExpected("type expression");
    return bld->inferExpr();
}

ast::TypeExprId Parser::parseOrExpr() {
    auto left = parsePrimaryType();

    // 'or' as identifier — check lexeme
    while (check(TokenKind::Identifier) && match("or")) {
        auto right = parsePrimaryType();

        // If left is not already a TypeSum, convert it
        auto &node = bld->getTypeExpr(left);
        if (auto *sum = std::get_if<ast::TypeSum>(&node)) {
            sum->members.push(right);
        } else {
            memory::DynArray<ast::TypeExprId> members{bld->arena()};
            members.push(left);
            members.push(right);
            left = bld->addTypeExpr(ast::TypeSum{std::move(members)});
        }
    }

    return left;
}

ast::TypeExprId Parser::parseTypeExpr() {
    // typeExpr → orExpr ('+' orExpr)*   (AND of constraints)
    // For now: '+' not yet implemented (requires trait system)
    // Just parse a single orExpr
    return parseOrExpr();
}

} // namespace zith::parser
