#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "parser/operators.hpp"

#include <cstdlib>
#include <string>

namespace zith::parser {

using lexer::TokenKind;
using namespace zith::diagnostics::err;
using namespace operators;

namespace {

using ast::kInvalidDecl;
using ast::kInvalidExpr;
using ast::kInvalidStmt;
using diagnostics::Severity;

bool matchBuiltinType(std::string_view name, ast::BuiltinType &out) {
    if (name == "i8")       { out = ast::BuiltinType::I8;    return true; }
    if (name == "i16")      { out = ast::BuiltinType::I16;   return true; }
    if (name == "i32")      { out = ast::BuiltinType::I32;   return true; }
    if (name == "i64")      { out = ast::BuiltinType::I64;   return true; }
    if (name == "i128")     { out = ast::BuiltinType::I128;  return true; }
    if (name == "u8")       { out = ast::BuiltinType::U8;    return true; }
    if (name == "u16")      { out = ast::BuiltinType::U16;   return true; }
    if (name == "u32")      { out = ast::BuiltinType::U32;   return true; }
    if (name == "u64")      { out = ast::BuiltinType::U64;   return true; }
    if (name == "u128")     { out = ast::BuiltinType::U128;  return true; }
    if (name == "f32")      { out = ast::BuiltinType::F32;   return true; }
    if (name == "f64")      { out = ast::BuiltinType::F64;   return true; }
    if (name == "bool")     { out = ast::BuiltinType::Bool;  return true; }
    if (name == "char")     { out = ast::BuiltinType::Char;  return true; }
    if (name == "void")     { out = ast::BuiltinType::Void;  return true; }
    if (name == "never" || name == "noreturn")
                            { out = ast::BuiltinType::Never; return true; }
    if (name == "invalid")  { out = ast::BuiltinType::Invalid; return true; }
    if (name == "unknown")  { out = ast::BuiltinType::Unknown; return true; }
    if (name == "opaque")   { out = ast::BuiltinType::Opaque; return true; }
    return false;
}

} // anonymous namespace

ast::TypeExprId Parser::parsePrimaryType() {
    // ── builtin types (i32, bool, void, …) ──────────────────────────
    if (peek().is(TokenKind::Type)) {
        ast::BuiltinType bt;
        if (matchBuiltinType(lexeme(), bt)) {
            advance();
            return bld->builtinExpr(bt);
        }
        // fall through: treat as a path
        memory::DynArray<std::string_view> segments{bld->arena()};
        segments.push(lexeme());
        advance();
        return bld->pathExpr(std::move(segments));
    }

    // ── pointer / ownership-qualified types ─────────────────────────
    {
        bool has_mut = false;
        ast::OwnershipKw ownership = ast::OwnershipKw::Default;

        if (peek().is(TokenKind::Mutable)) {
            has_mut = true;
            advance();
        }

        if (peek().is(TokenKind::Ownership)) {
            auto kw = lexeme();
            if (kw == "unique")  ownership = ast::OwnershipKw::Unique;
            if (kw == "share")   ownership = ast::OwnershipKw::Share;
            if (kw == "lend")    ownership = ast::OwnershipKw::Lend;
            if (kw == "view")    ownership = ast::OwnershipKw::View;
            if (kw == "belong")  ownership = ast::OwnershipKw::Belong;
            advance();

            // qualifier without '*': lend T, share T, etc.
            if (peek().punc != '*') {
                auto inner = parsePrimaryType();
                return bld->addTypeExpr(ast::TypePtrExpr{inner, has_mut, ownership});
            }
        }

        if (peek().punc == '*') {
            advance();
            auto inner = parsePrimaryType();
            return bld->addTypeExpr(ast::TypePtrExpr{inner, has_mut, ownership});
        }

        if (has_mut || ownership != ast::OwnershipKw::Default) {
            diag->report(diagnostics::Severity::Error, diagnostics::err::ExpectedExpr,
                         "expected pointer type after 'mut'", peek().span);
            return bld->inferExpr();
        }
    }

    // ── optional type: ?T ───────────────────────────────────────────
    if (peek().punc == '?') {
        advance();
        auto inner = parsePrimaryType();
        return bld->addTypeExpr(ast::TypeOptional{inner});
    }

    // ── pack type: |T, U| or |name: T, U| ─────────────────────────
    if (peek().punc == '|') {
        advance();
        memory::DynArray<ast::TypePackMember> members{bld->arena()};
        while (!peek().is_eof() && peek().punc != '|') {
            if (peek().is(TokenKind::Identifier)) {
                // lookahead: if next token is ':', it's a named member
                if (peek(1).punc == ':') {
                    auto name = lexeme();
                    advance(); // name
                    advance(); // ':'
                    auto type = parsePrimaryType();
                    members.push({name, type});
                } else {
                    auto type = parsePrimaryType();
                    members.push({std::string_view{}, type});
                }
            } else {
                // positional — no name needed for packs starting with '(' or type kw
                auto type = parsePrimaryType();
                members.push({std::string_view{}, type});
            }
            if (peek().punc == ',')
                advance();
        }
        if (peek().punc == '|')
            advance();
        return bld->addTypeExpr(ast::TypePack{std::move(members)});
    }

    // ── parenthesized: (T) or fn(...) ─────────────────────────────
    if (peek().punc == '(') {
        advance();
        auto inner = parseTypeExpr();
        if (peek().punc == ')')
            advance();
        return inner;
    }

    // ── slice / array: []T or [N]T ────────────────────────────────
    if (peek().punc == '[') {
        advance();
        if (peek().punc == ']') {
            advance();
            auto elem = parsePrimaryType();
            return bld->addTypeExpr(ast::TypeSlice{elem});
        }
        // [N]T — parse count expression then type
        // For now: count is an integer literal
        auto count_expr = bld->inferExpr(); // placeholder
        while (!peek().is_eof() && peek().punc != ']')
            advance();
        if (peek().punc == ']')
            advance();
        auto elem = parsePrimaryType();
        return bld->addTypeExpr(ast::TypeArray{elem, count_expr});
    }

    // ── fn type: fn(T): U ─────────────────────────────────────────
    if (peek().is(TokenKind::Fn)) {
        advance();
        memory::DynArray<ast::TypeExprId> params{bld->arena()};
        if (peek().punc == '(') {
            advance();
            while (!peek().is_eof() && peek().punc != ')') {
                params.push(parseTypeExpr());
                if (peek().punc == ',')
                    advance();
            }
            if (peek().punc == ')')
                advance();
        }
        auto ret = bld->inferExpr();
        if (peek().punc == ':') {
            advance();
            ret = parseTypeExpr();
        }
        return bld->addTypeExpr(ast::TypeFnExpr{std::move(params), ret});
    }

    // ── infer type: _ ──────────────────────────────────────────────
    if (peek().punc == '_') {
        advance();
        return bld->inferExpr();
    }

    // ── identifier / path: Vec, std.vec.Vec ────────────────────────
    if (peek().is(TokenKind::Identifier)) {
        auto id_span = peek().span;
        auto id = lexeme();
        // check for builtin types that aren't TokenKind::Type (never, opaque)
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
        // qualified path: std.vec.Vec
        while (peek().punc == '.') {
            advance();
            if (peek().is(TokenKind::Identifier)) {
                path_span.end = peek().span.end;
                segments.push(lexeme());
                advance();
            } else {
                break;
            }
        }
        return bld->pathExpr(std::move(segments), path_span);
    }

    diag->report(diagnostics::Severity::Error, diagnostics::err::ExpectedExpr,
                 "expected type expression", peek().span);
    return bld->inferExpr();
}

ast::TypeExprId Parser::parseOrExpr() {
    auto left = parsePrimaryType();

    // 'or' as identifier — check lexeme
    while (peek().is(TokenKind::Identifier) && lexeme() == "or") {
        advance();
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
