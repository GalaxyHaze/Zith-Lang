#include "parser.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-table.hpp"
#include "lexer/lexer.hpp"
#include "memory/source-map.hpp"
#include "parser/operators.hpp"
#include "parser/scan-helpers.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace zith::parser {

namespace {

using ast::kInvalidDecl;
using ast::kInvalidExpr;
using ast::kInvalidStmt;
using diagnostics::Severity;
using lexer::TokenKind;
using namespace zith::diagnostics::err;
using namespace operators;

} // anonymous namespace

// ── token helpers ──────────────────────────────────────────────────────

std::string_view Parser::lexeme() {
    auto &s = tok->peek().span;
    return {tok->file_base + s.start, s.end - s.start};
}

const lexer::Token &Parser::peek() {
    return tok->peek();
}
const lexer::Token &Parser::peek(uint32_t n) {
    return tok->peek(n);
}
void Parser::advance() {
    tok->advance();
}
void Parser::advance(uint32_t n) {
    tok->advance(n);
}
bool Parser::eof() {
    return peek().is_eof();
}

bool Parser::match(TokenKind kind) {
    return tok->match(kind);
}

bool Parser::expect(TokenKind kind) {
    if (match(kind))
        return true;
    diag->report(Severity::Error, ExpectedExpr,
                 std::string("expected ") + lexer::tokenKindName(kind) + " but got " +
                     lexer::tokenKindName(peek().kind),
                 peek().span);
    return false;
}

bool Parser::consume(char c) {
    if (tok->peek().punc == c) {
        advance();
        return true;
    }
    return false;
}

bool Parser::consume(TokenKind kind) {
    if (match(kind)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::expectPunc(char c) {
    if (peek().punc == c) {
        advance();
        return true;
    }
    std::string msg = "expected '";
    msg += c;
    msg += "'";
    diag->report(Severity::Error, ExpectedExpr, std::move(msg), peek().span);
    return false;
}

bool Parser::expectIdent(std::string_view &out) {
    if (!peek().is(TokenKind::Identifier)) {
        diag->report(Severity::Error, ExpectedIdent, "expected identifier", peek().span);
        return false;
    }
    out = lexeme();
    advance();
    return true;
}

std::string_view Parser::expectIdent() {
    std::string_view name;
    if (!expectIdent(name))
        recovery::panic(*tok, {TokenKind::End, TokenKind::Punctuation});
    return name;
}

memory::Span Parser::spanFrom(memory::Span start) const {
    if (tok->offset > 0) {
        auto &prev = tok->src[tok->offset - 1];
        return {start.file, start.start, prev.span.end};
    }
    return start;
}

memory::Span Parser::spanFrom(ast::ExprId lhs, ast::ExprId rhs) const {
    auto ls = bld->exprSpan(lhs);
    auto rs = (rhs != ast::kInvalidExpr) ? bld->exprSpan(rhs) : ls;
    return {ls.file, ls.start, rs.end};
}

// ── comment skipping helper ────────────────────────────────────────────

static void skipComments(lexer::TokenStream &tok) {
    while (!tok.is_empty()) {
        auto &t = tok.peek();
        if (t.is_not(lexer::TokenKind::Comments) && t.is_not(lexer::TokenKind::Docs))
            break;
        tok.advance();
    }
}

// ── expression parsing ─────────────────────────────────────────────────

ast::ExprId Parser::parsePrimary() {
    skipComments(*tok);
    if (consume('{'))
        return parseBlock();

    // 'if' expression
    if (peek().is(TokenKind::If) && lexeme() == "if") {
        auto if_span = peek().span;
        advance();
        auto cond               = parseExpr();
        ast::ExprId then_branch = kInvalidExpr;
        if (peek().punc == '{') {
            advance();
            then_branch = parseBlock();
        }
        ast::ExprId else_branch = kInvalidExpr;
        if (peek().is(TokenKind::If) && lexeme() == "else") {
            advance();
            if (peek().is(TokenKind::If) && lexeme() == "if")
                else_branch = parsePrimary();
            else if (peek().punc == '{') {
                advance();
                else_branch = parseBlock();
            }
        }
        return bld->ifExpr(cond, then_branch, else_branch, spanFrom(if_span));
    }

    // 'while' expression
    if (peek().is(TokenKind::Control) && lexeme() == "while") {
        auto while_span = peek().span;
        advance();
        auto cond        = parseExpr();
        ast::ExprId body = kInvalidExpr;
        if (peek().punc == '{') {
            advance();
            body = parseBlock();
        }
        return bld->whileExpr(cond, body, spanFrom(while_span));
    }

    // 'for' expression — desugar into while
    if (peek().is(TokenKind::For) && lexeme() == "for") {
        advance();
        // for (init; cond; step) body
        // desugar: { init; while cond { body; step; } }
        auto desugar_block = bld->block(memory::DynArray<ast::StmtId>{bld->arena()});
        auto &block        = std::get<ast::BlockNode>(bld->getExpr(desugar_block));

        if (peek().punc == '(') {
            advance();
            // init statement
            if (peek().is(TokenKind::Variable)) {
                advance();
                if (peek().is(TokenKind::Identifier)) {
                    auto name = lexeme();
                    advance();
                    ast::ExprId init_val = kInvalidExpr;
                    if (peek().punc == '=') {
                        advance();
                        init_val = parseExpr();
                    }
                    block.stmts.push(bld->letStmt(name, false, init_val));
                }
            } else if (peek().punc != ';') {
                block.stmts.push(bld->addStmt(parseExpr()));
            }
            consume(';');

            // condition (default true)
            ast::ExprId cond = kInvalidExpr;
            if (peek().punc != ';') {
                cond = parseExpr();
            }
            consume(';');

            // step expression
            ast::ExprId step = kInvalidExpr;
            if (peek().punc != ')') {
                step = parseExpr();
            }
            consume(')');

            // body
            ast::ExprId loop_body = kInvalidExpr;
            if (peek().punc == '{') {
                advance();
                loop_body = parseBlock();
            }

            // Build while cond { body; step; }
            auto body_block = bld->block(memory::DynArray<ast::StmtId>{bld->arena()});
            auto &bb        = std::get<ast::BlockNode>(bld->getExpr(body_block));
            if (loop_body != kInvalidExpr) {
                auto &lb = std::get<ast::BlockNode>(bld->getExpr(loop_body));
                for (auto s : lb.stmts)
                    bb.stmts.push(s);
                bb.trailing = lb.trailing;
            }
            if (step != kInvalidExpr)
                bb.stmts.push(bld->addStmt(step));

            ast::ExprId while_cond =
                cond != kInvalidExpr ? cond : bld->litExpr(ast::LitKind::Bool, "true");
            auto while_expr = bld->whileExpr(while_cond, body_block);
            block.stmts.push(bld->addStmt(while_expr));
        }

        return desugar_block;
    }

    if (peek().is(TokenKind::LitVal)) {
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

    if (peek().is(TokenKind::Identifier)) {
        auto id_span = peek().span;
        auto name    = lexeme();
        advance();
        return bld->ident(name, id_span);
    }

    if (peek().punc == '(') {
        advance();
        auto inner = parseExpr();
        if (!consume(')'))
            diag->report(Severity::Error, UnclosedParen, "expected ')'", peek().span);
        return inner;
    }

    diag->report(Severity::Error, ExpectedExpr, "expected expression", peek().span);
    advance();
    return kInvalidExpr;
}

ast::ExprId Parser::parsePrefix() {
    // Unary prefix operators
    if (peek().punc == '-') {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Neg, operand, spanFrom(op_span));
    }
    if (peek().punc == '!') {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Not, operand, spanFrom(op_span));
    }
    if (peek().punc == '&') {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Ref, operand, spanFrom(op_span));
    }
    if (peek().punc == '*') {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Deref, operand, spanFrom(op_span));
    }
    // word prefix 'not'
    if (peek().is(TokenKind::Logical) && lexeme() == "not") {
        auto op_span = peek().span;
        advance();
        auto operand = parsePrefix();
        return bld->unary(ast::UnaryOp::Not, operand, spanFrom(op_span));
    }

    return parsePrimary();
}

ast::ExprId Parser::parseExpr(int min_prec) {
    auto lhs = parsePrefix();

    for (;;) {
        if (eof())
            break;

        if (lhs == kInvalidExpr)
            break;

        auto &cur = peek();

        // ── Postfix: call ( args ) ──────────────────────────────
        if (cur.punc == '(') {
            auto lhs_span = bld->exprSpan(lhs);
            advance();
            auto args = parseCommaList(')', [&]{ return parseExpr(); });
            if (!consume(')'))
                diag->report(Severity::Error, UnclosedParen, "expected ')'", peek().span);
            auto end_span = tok->src[tok->offset - 1].span;
            lhs = bld->call(lhs, std::move(args),
                            memory::Span{lhs_span.file, lhs_span.start, end_span.end});
            continue;
        }

        // ── Postfix: ..range ────────────────────────────────────
        if (cur.punc == '.' && peek(1).punc == '.') {
            advance(2);
            auto rhs = parseExpr(min_prec + 1);
            lhs = bld->range(lhs, rhs, spanFrom(lhs, rhs));
            continue;
        }

        // ── Postfix: .field ─────────────────────────────────────
        if (cur.punc == '.') {
            auto lhs_span = bld->exprSpan(lhs);
            advance();
            auto field_span = peek().span;
            std::string_view field;
            if (!expectIdent(field))
                continue;
            lhs = bld->field(lhs, field,
                             memory::Span{lhs_span.file, lhs_span.start, field_span.end});
            continue;
        }

        // ── Postfix: ->field (member access by pointer) ─────────
        if (cur.is(lexer::TokenKind::Operators) && cur.punc == '-' && peek(1).punc == '>') {
            auto lhs_span = bld->exprSpan(lhs);
            advance(2);
            auto field_span = peek().span;
            std::string_view field;
            if (!expectIdent(field))
                continue;
            lhs = bld->field(lhs, field,
                             memory::Span{lhs_span.file, lhs_span.start, field_span.end});
            continue;
        }

        // ── Postfix: [index] ────────────────────────────────────
        if (cur.punc == '[') {
            auto lhs_span = bld->exprSpan(lhs);
            advance();
            auto index = parseExpr();
            if (index != kInvalidExpr && !consume(']'))
                diag->report(Severity::Error, ExpectedExpr, "expected ']'", peek().span);
            auto end_span = tok->src[tok->offset - 1].span;
            lhs = bld->index(lhs, index,
                             memory::Span{lhs_span.file, lhs_span.start, end_span.end});
            continue;
        }

        // ── Postfix: unwrap ! ───────────────────────────────────
        if (cur.is(lexer::TokenKind::Operators) && cur.punc == '!' &&
            !peek(1).is(lexer::TokenKind::Operators)) {
            auto lhs_span = bld->exprSpan(lhs);
            advance();
            auto end_span = tok->src[tok->offset - 1].span;
            lhs = bld->unary(ast::UnaryOp::Deref, lhs,
                             memory::Span{lhs_span.file, lhs_span.start, end_span.end});
            continue;
        }

        // ── Word infix: and / or / xor ──────────────────────────
        if (cur.is(lexer::TokenKind::Logical)) {
            auto word    = lexeme();
            uint8_t prec = wordPrec(word);
            if (prec == 0 || prec < min_prec)
                break;
            advance();
            auto rhs = parseExpr(prec + 1);
            lhs = bld->binary(lhs, binaryOpForWord(word), rhs, spanFrom(lhs, rhs));
            continue;
        }

        // ── Compound operator: ==, !=, <=, >=, <<, >> ───
        ast::BinaryOp compound_op;
        if (tryCompoundOp(cur, peek(1), compound_op)) {
            uint8_t prec = infixPrec(cur);
            if (prec < min_prec)
                break;
            advance(2);
            auto rhs = parseExpr(prec + 1);
            lhs = bld->binary(lhs, compound_op, rhs, spanFrom(lhs, rhs));
            continue;
        }

        // ── Infix binary operator (single char) ─────────────────
        uint8_t prec = infixPrec(cur);
        if (prec == 0 || prec < min_prec)
            break;

        // Check that it's really an infix operator, not part of a compound
        ast::BinaryOp compound_check;
        if (tryCompoundOp(cur, peek(1), compound_check)) {
            break;
        }

        advance();
        auto rhs = parseExpr(prec + 1);
        lhs = bld->binary(lhs, binaryOpForChar(cur.punc), rhs, spanFrom(lhs, rhs));
    }

    return lhs;
}

ast::ExprId Parser::parseExpr() {
    return parseExpr(0);
}

ast::ExprId Parser::parseBlock() {
    auto start_span = peek().span;
    memory::DynArray<ast::StmtId> stmts{bld->arena()};

    while (!eof()) {
        skipComments(*tok);
        if (peek().punc == '}')
            break;
        stmts.push(parseStmt());
    }

    if (peek().punc == '}')
        advance();
    return bld->block(std::move(stmts), kInvalidExpr, spanFrom(start_span));
}

// ── statement parsing ──────────────────────────────────────────────────

ast::StmtId Parser::parseStmt() {
    skipComments(*tok);
    if (peek().is(TokenKind::Control) && lexeme() == "return") {
        auto ret_span = peek().span;
        advance();
        auto val = eof() || peek().punc == '}' ? kInvalidExpr : parseExpr();
        if (peek().punc != ';')
            skipUntil({TokenKind::End, TokenKind::Punctuation});
        else
            advance();
        return bld->retStmt(val, spanFrom(ret_span));
    }

    if (peek().is(TokenKind::Control) && lexeme() == "break") {
        auto br_span = peek().span;
        advance();
        if (peek().punc == ';')
            advance();
        auto nil = bld->litExpr(ast::LitKind::Nil, "null", peek().span);
        return bld->addStmt(nil);
    }

    if (peek().is(TokenKind::Control) && lexeme() == "continue") {
        auto cont_span = peek().span;
        advance();
        if (peek().punc == ';')
            advance();
        auto nil = bld->litExpr(ast::LitKind::Nil, "null", peek().span);
        return bld->addStmt(nil);
    }

    if (peek().is(TokenKind::Variable)) {
        auto let_span = peek().span;
        advance();
        auto name = expectIdent();
        auto type_annot = parseOptTypeAnnotation();
        ast::ExprId init = kInvalidExpr;
        if (consume('='))
            init = parseExpr();
        if (peek().punc == ';')
            advance();
        memory::DynArray<std::string_view> names{bld->arena()};
        names.push(name);
        return bld->letStmt(std::move(names), false, type_annot, init, spanFrom(let_span));
    }

    auto expr = parseExpr();
    if (peek().punc == '=') {
        auto assign_span = peek().span;
        advance();
        auto rhs = parseExpr();
        if (peek().punc == ';')
            advance();
        return bld->assign(expr, rhs, spanFrom(assign_span));
    }
    if (peek().punc == ';')
        advance();
    return bld->addStmt(expr);
}

// ── declaration parsing ────────────────────────────────────────────────

ast::DeclId Parser::parseFnDecl() {
    if (!peek().is(TokenKind::Identifier)) {
        diag->report(Severity::Error, ExpectedIdent, "expected function name", peek().span);
        return kInvalidDecl;
    }
    auto name_span = peek().span;
    auto name      = lexeme();
    advance();

    if (!consume('(')) {
        diag->report(Severity::Error, ExpectedExpr, "expected '(' after function name",
                     peek().span);
        return kInvalidDecl;
    }

    memory::DynArray<std::string_view> params{bld->arena()};
    while (!eof() && peek().punc != ')') {
        if (!peek().is(TokenKind::Identifier)) {
            diag->report(Severity::Error, ExpectedIdent, "expected parameter name", peek().span);
            advance();
            continue;
        }
        params.push(lexeme());
        advance();
        if (peek().punc == ',')
            advance();
    }
    consume(')');

    ast::ExprId body = kInvalidExpr;
    if (peek().punc == '{')
        body = parseBlock();

    return bld->fnDecl(name, std::move(params), body, spanFrom(name_span));
}

// ── scan (first pass: register symbols, skip bodies) ───────────────────

namespace {

using namespace scan_detail;

} // anonymous namespace

ScanResult scan(Parser &parser, import::SymbolTable &syms) {
    auto &tok     = *parser.tok;
    auto &bld     = *parser.bld;
    auto &diag    = *parser.diag;
    auto &program = parser.program;

    ScanResult result{bld.arena()};
    import::SymbolVisibility current_vis = import::SymbolVisibility::Private;
    int32_t current_mod_depth            = 0;

    memory::Span lastDocSpan{};

    while (!tok.is_empty()) {
        if (tok.peek().is_eof())
            break;

        // ── capture doc comments ───────────────────────────────────
        if (tok.peek().is(TokenKind::Docs)) {
            if (lastDocSpan.start == 0 && lastDocSpan.end == 0)
                lastDocSpan = tok.peek().span;
            else
                lastDocSpan.end = tok.peek().span.end;
            tok.advance();
            continue;
        }

        // ── skip plain comments ────────────────────────────────────
        if (tok.peek().is(TokenKind::Comments)) {
            tok.advance();
            continue;
        }

        // ── visibility modifier (pub / mod) ────────────────────────
        if (tok.peek().is(TokenKind::Visibility)) {
            auto kw = tok.lexeme();
            tok.advance();

            if (kw == "pub") {
                current_vis = import::SymbolVisibility::Public;
                continue;
            }

            if (kw == "mod") {
                current_vis = import::SymbolVisibility::Module;
                if (tok.peek().punc == '(') {
                    tok.advance();
                    if (tok.peek().punc == '.') {
                        tok.advance(); // '.'
                        tok.advance(); // '.'
                        current_mod_depth = -1;
                        if (tok.peek().punc == ')')
                            tok.advance();
                    } else if (tok.peek().is(TokenKind::LitVal)) {
                        auto n = tok.lexeme();
                        tok.advance();
                        char *end         = nullptr;
                        long val          = std::strtol(n.data(), &end, 10);
                        current_mod_depth = (end > n.data() && val >= 1 && val <= INT32_MAX)
                                                ? static_cast<int32_t>(val)
                                                : 1;
                        if (tok.peek().punc == ')')
                            tok.advance();
                    } else {
                        current_mod_depth = 1;
                        if (tok.peek().punc == ')')
                            tok.advance();
                    }
                } else {
                    current_mod_depth = 1;
                }
                continue;
            }
        }

        // ── fn declaration ─────────────────────────────────────────
        if (tok.peek().is(TokenKind::Fn)) {
            tok.advance();

            if (!tok.peek().is(TokenKind::Identifier)) {
                diag.report(Severity::Error, ExpectedIdent, "expected function name",
                            tok.peek().span);
                tok.advance();
                current_vis       = import::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            if (tok.peek().punc != '(') {
                diag.report(Severity::Error, ExpectedExpr, "expected '(' after function name",
                            tok.peek().span);
                current_vis       = import::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }
            tok.advance();
            uint32_t param_token_start = tok.offset;

            memory::DynArray<std::string_view> params{bld.arena()};
            memory::DynArray<memory::Span> param_spans{bld.arena()};
            while (!tok.is_empty()) {
                if (tok.peek().is_eof())
                    break;
                if (tok.peek().punc == ')')
                    break;

                // destructured params: [a, b, c]: Type
                if (tok.peek().punc == '[') {
                    tok.advance(); // consume '['
                    while (!tok.is_empty() && tok.peek().punc != ']') {
                        if (tok.peek().is(TokenKind::Identifier)) {
                            param_spans.push(tok.peek().span);
                            params.push(tok.lexeme());
                            tok.advance();
                        } else {
                            tok.advance();
                        }
                        if (tok.peek().punc == ',')
                            tok.advance();
                    }
                    if (tok.peek().punc == ']')
                        tok.advance();
                    // skip type annotation
                    if (tok.peek().punc == ':') {
                        tok.advance();
                        skipTypeExpr(tok);
                    }
                    if (tok.peek().punc == ',')
                        tok.advance();
                    continue;
                }

                if (!tok.peek().is(TokenKind::Identifier)) {
                    diag.report(Severity::Error, ExpectedIdent, "expected parameter name",
                                tok.peek().span);
                    tok.advance();
                    continue;
                }
                param_spans.push(tok.peek().span);
                params.push(tok.lexeme());
                tok.advance();

                // skip type annotation : TypeExpr
                if (tok.peek().punc == ':') {
                    tok.advance();
                    skipTypeExpr(tok);
                }

                if (tok.peek().punc == ',')
                    tok.advance();
            }
            if (tok.peek().punc == ')')
                tok.advance();

            // skip return type annotation : TypeExpr
            if (tok.peek().punc == ':') {
                tok.advance();
                skipTypeExpr(tok);
            }

            ast::ExprId body_node = kInvalidExpr;
            uint32_t token_start  = 0;
            uint32_t token_end    = 0;
            memory::Span body_span{};

            if (tok.peek().punc == '{') {
                token_start = tok.offset;
                body_span   = tok.peek().span;
                token_end   = skipBody(tok);
                body_span   = {body_span.file, body_span.start, token_end};
                body_node   = bld.unbody(body_span, token_start, token_end);
            }

            auto fn_sym = syms.declare(name, current_vis, current_mod_depth, import::SymKind::Fn,
                                       ast::kInvalidDecl, name_span, import::kInvalidSym,
                                       lastDocSpan);
            for (size_t i = 0; i < params.size(); i++) {
                auto ps = syms.declare(params[i], current_vis, current_mod_depth,
                                       import::SymKind::Variable,
                                       ast::kInvalidDecl, param_spans[i]);
                syms.get(fn_sym).members.push(ps);
            }
            lastDocSpan = {};

            auto decl = bld.fnDecl(name, std::move(params), body_node, spanFromOffset(name_span.start, name_span.end));
            program.decls.push(decl);
            if (fn_sym != import::kInvalidSym)
                syms.get(fn_sym).decl_id = decl;

            result.fns.push({name, body_span, body_node, param_token_start});
            current_vis       = import::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── struct-like declaration (struct / enum / union / component) ─
        if (tok.peek().is(TokenKind::Struct)) {
            auto kw = tok.lexeme();
            tok.advance();

            if (!tok.peek().is(TokenKind::Identifier)) {
                diag.report(Severity::Error, ExpectedIdent,
                            "expected name after '" + std::string(kw) + "'", tok.peek().span);
                tok.advance();
                current_vis       = import::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            import::SymKind sk;
            if (kw == "enum")
                sk = import::SymKind::Enum;
            else if (kw == "union")
                sk = import::SymKind::Union;
            else if (kw == "component")
                sk = import::SymKind::Component;
            else
                sk = import::SymKind::Struct;

            auto decl             = ast::kInvalidDecl;
            ast::ExprId body_node = kInvalidExpr;
            memory::Span body_span{};

            // create appropriate AST node
            if (kw == "struct") {
                decl = bld.structDecl(name, memory::DynArray<ast::StructField>(bld.arena()),
                                      name_span);
            } else if (kw == "enum") {
                decl = bld.enumDecl(name, memory::DynArray<ast::EnumVariant>(bld.arena()),
                                    name_span);
            } else if (kw == "union") {
                decl = bld.unionDecl(name, memory::DynArray<ast::UnionVariant>(bld.arena()),
                                     name_span);
            } else {
                decl = bld.componentDecl(name, name_span);
            }

            // ── scan body: register member names, skip implementations ──
            if (tok.peek().punc == '{') {
                uint32_t token_start = tok.offset;
                body_span            = tok.peek().span;

                if (kw == "struct") {
                    // scan field / method names inside struct body
                    tok.advance(); // consume '{'
                    while (!tok.is_empty()) {
                        if (tok.peek().is_eof() || tok.peek().punc == '}')
                            break;

                        // visibility modifier inside body
                        if (tok.peek().is(TokenKind::Visibility)) {
                            auto vkw = tok.lexeme();
                            tok.advance();
                            if (vkw == "mod" && tok.peek().punc == '(')
                                skipBalanced(tok, '(', ')');
                            continue;
                        }

                        // method: fn / async fn / raw fn / const fn / flowing fn
                        if (tok.peek().is(TokenKind::Fn) ||
                            tok.peek().is(TokenKind::Control) /* flowing */) {
                            if (tok.peek().is(TokenKind::Control))
                                tok.advance(); // 'flowing'
                            if (tok.peek().is(TokenKind::Fn))
                                tok.advance();
                            if (tok.peek().is(TokenKind::Identifier)) {
                                auto mname_span = tok.peek().span;
                                auto mname      = tok.lexeme();
                                tok.advance();
                                // register method name as fn member
                                if (!reportIfDuplicate(syms, diag, mname, mname_span)) {
                                    auto ms = syms.declare(mname, current_vis, current_mod_depth,
                                                           import::SymKind::Fn,
                                                           ast::kInvalidDecl, mname_span);
                                    if (decl != ast::kInvalidDecl && ms != import::kInvalidSym)
                                        syms.get(ms).decl_id = decl;
                                }
                            }
                            // skip to ';' or '{' ... '}'
                            while (!tok.is_empty() && !tok.peek().is_eof()) {
                                if (tok.peek().punc == ';') {
                                    tok.advance();
                                    break;
                                }
                                if (tok.peek().punc == '{') {
                                    skipBalanced(tok, '{', '}');
                                    break;
                                }
                                if (tok.peek().punc == '}')
                                    break;
                                tok.advance();
                            }
                            continue;
                        }

                        // destructure fields: [a, b, c]: Type
                        if (tok.peek().punc == '[') {
                            tok.advance();
                            while (!tok.is_empty() && tok.peek().punc != ']') {
                                if (tok.peek().is(TokenKind::Identifier)) {
                                    auto fname_span = tok.peek().span;
                                    auto fname      = tok.lexeme();
                                    tok.advance();
                                    if (!reportIfDuplicate(syms, diag, fname, fname_span)) {
                                        auto fs =
                                            syms.declare(fname, import::SymbolVisibility::Private,
                                                         0, import::SymKind::Variable,
                                                         ast::kInvalidDecl, fname_span);
                                        if (decl != ast::kInvalidDecl && fs != import::kInvalidSym)
                                            syms.get(fs).decl_id = decl;
                                    }
                                } else {
                                    tok.advance();
                                }
                                if (tok.peek().punc == ',')
                                    tok.advance();
                            }
                            if (tok.peek().punc == ']')
                                tok.advance();
                            // skip type annotation
                            if (tok.peek().punc == ':') {
                                tok.advance();
                                skipTypeExpr(tok);
                            }
                            // skip default value
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                skipExpr(tok);
                            }
                            if (tok.peek().punc == ',')
                                tok.advance();
                            continue;
                        }

                        // field: ident : Type = default ,
                        if (tok.peek().is(TokenKind::Identifier)) {
                            auto fname_span = tok.peek().span;
                            auto fname      = tok.lexeme();
                            tok.advance();
                            if (!reportIfDuplicate(syms, diag, fname, fname_span)) {
                                auto fs = syms.declare(fname, import::SymbolVisibility::Private, 0,
                                                       import::SymKind::Variable,
                                                       ast::kInvalidDecl, fname_span);
                                if (decl != ast::kInvalidDecl && fs != import::kInvalidSym)
                                    syms.get(fs).decl_id = decl;
                            }
                            // skip type annotation
                            if (tok.peek().punc == ':') {
                                tok.advance();
                                skipTypeExpr(tok);
                            }
                            // skip default value
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                skipExpr(tok);
                            }
                            if (tok.peek().punc == ',')
                                tok.advance();
                            continue;
                        }

                        // anything else — skip
                        tok.advance();
                    }
                    if (tok.peek().punc == '}')
                        tok.advance(); // consume '}'

                } else if (kw == "enum") {
                    // scan variant names inside enum body
                    tok.advance(); // consume '{'
                    while (!tok.is_empty()) {
                        if (tok.peek().is_eof() || tok.peek().punc == '}')
                            break;

                        if (tok.peek().is(TokenKind::Identifier)) {
                            auto v_span = tok.peek().span;
                            auto vname  = tok.lexeme();
                            tok.advance();

                            if (!reportIfDuplicate(syms, diag, vname, v_span)) {
                                auto vs = syms.declare(vname, import::SymbolVisibility::Private, 0,
                                                       import::SymKind::Variable,
                                                       ast::kInvalidDecl, v_span);
                                if (decl != ast::kInvalidDecl && vs != import::kInvalidSym)
                                    syms.get(vs).decl_id = decl;
                            }

                            // tuple variant: Variant(Type1, Type2)
                            if (tok.peek().punc == '(')
                                skipBalanced(tok, '(', ')');
                            // struct variant: Variant { field: Type }
                            if (tok.peek().punc == '{')
                                skipBalanced(tok, '{', '}');
                            // discriminant assignment: Variant = 1
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                skipExpr(tok);
                            }
                        } else {
                            tok.advance();
                        }

                        if (tok.peek().punc == ',')
                            tok.advance();
                    }
                    if (tok.peek().punc == '}')
                        tok.advance(); // consume '}'

                } else if (kw == "union") {
                    // scan variant names inside union body
                    tok.advance(); // consume '{'
                    while (!tok.is_empty()) {
                        if (tok.peek().is_eof() || tok.peek().punc == '}')
                            break;

                        if (tok.peek().is(TokenKind::Identifier)) {
                            auto v_span = tok.peek().span;
                            auto vname  = tok.lexeme();
                            tok.advance();

                            if (!reportIfDuplicate(syms, diag, vname, v_span)) {
                                auto vs = syms.declare(vname, import::SymbolVisibility::Private, 0,
                                                       import::SymKind::Variable,
                                                       ast::kInvalidDecl, v_span);
                                if (decl != ast::kInvalidDecl && vs != import::kInvalidSym)
                                    syms.get(vs).decl_id = decl;
                            }

                            // skip type annotation: variant: Type
                            if (tok.peek().punc == ':') {
                                tok.advance();
                                skipTypeExpr(tok);
                            }
                            // skip default
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                skipExpr(tok);
                            }
                        } else {
                            tok.advance();
                        }

                        if (tok.peek().punc == ',')
                            tok.advance();
                    }
                    if (tok.peek().punc == '}')
                        tok.advance(); // consume '}'

                } else if (kw == "component") {
                    // component body: skip entirely during scan
                    skipBalanced(tok, '{', '}');
                }

                body_span = {body_span.file, body_span.start, tok.offset};
                body_node = bld.unbody(body_span, token_start, tok.offset);
            }

            program.decls.push(decl);
            if (!reportIfDuplicate(syms, diag, name, name_span))
                syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span,
                             import::kInvalidSym, lastDocSpan);
            lastDocSpan = {};

            ScanEntry entry{name, body_span, body_node};
            if (kw == "struct")
                result.structs.push(entry);
            else if (kw == "enum")
                result.enums.push(entry);
            else if (kw == "union")
                result.unions.push(entry);
            else if (kw == "component")
                result.components.push(entry);

            current_vis       = import::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── trait / interface declaration ────────────────────────────
        if (tok.peek().is(TokenKind::Trait) || tok.peek().is(TokenKind::Interface)) {
            auto kw = tok.lexeme();
            tok.advance();

            if (!tok.peek().is(TokenKind::Identifier)) {
                diag.report(Severity::Error, ExpectedIdent,
                            "expected name after '" + std::string(kw) + "'", tok.peek().span);
                tok.advance();
                current_vis       = import::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            import::SymKind sk =
                (kw == "interface") ? import::SymKind::Interface : import::SymKind::Trait;
            auto decl =
                (kw == "interface")
                    ? bld.interfaceDecl(name, memory::DynArray<ast::TraitMethod>(bld.arena()),
                                        name_span)
                    : bld.traitDecl(name, memory::DynArray<ast::TraitMethod>(bld.arena()),
                                    name_span);

            ast::ExprId body_node = kInvalidExpr;
            memory::Span body_span{};

            // ── scan trait body: register method names ──
            if (tok.peek().punc == '{') {
                uint32_t token_start = tok.offset;
                body_span            = tok.peek().span;

                tok.advance(); // consume '{'
                while (!tok.is_empty()) {
                    if (tok.peek().is_eof() || tok.peek().punc == '}')
                        break;

                    // fn / async fn / raw fn
                    if (tok.peek().is(TokenKind::Fn)) {
                        tok.advance();
                        if (tok.peek().is(TokenKind::Identifier)) {
                            auto m_span = tok.peek().span;
                            auto mname  = tok.lexeme();
                            tok.advance();
                            if (!reportIfDuplicate(syms, diag, mname, m_span)) {
                                auto ms = syms.declare(mname, import::SymbolVisibility::Private, 0,
                                                       import::SymKind::Fn,
                                                       ast::kInvalidDecl, m_span);
                                if (decl != ast::kInvalidDecl && ms != import::kInvalidSym)
                                    syms.get(ms).decl_id = decl;
                            }
                        }
                        // skip to ';' or default body
                        while (!tok.is_empty() && !tok.peek().is_eof()) {
                            if (tok.peek().punc == ';') {
                                tok.advance();
                                break;
                            }
                            if (tok.peek().punc == '{') {
                                skipBalanced(tok, '{', '}');
                                break;
                            }
                            if (tok.peek().punc == '}')
                                break;
                            tok.advance();
                        }
                        continue;
                    }

                    // associated type: type Foo;
                    if (tok.peek().is(TokenKind::Typedef)) {
                        tok.advance();
                        while (!tok.is_empty() && !tok.peek().is_eof() && tok.peek().punc != ';')
                            tok.advance();
                        if (tok.peek().punc == ';')
                            tok.advance();
                        continue;
                    }

                    tok.advance();
                }
                if (tok.peek().punc == '}')
                    tok.advance(); // consume '}'

                body_span = {body_span.file, body_span.start, tok.offset};
                body_node = bld.unbody(body_span, token_start, tok.offset);
            }

            program.decls.push(decl);
            if (!reportIfDuplicate(syms, diag, name, name_span))
                syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span,
                             import::kInvalidSym, lastDocSpan);
            lastDocSpan = {};

            result.traits.push({name, body_span, body_node});
            current_vis       = import::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── implement declaration ────────────────────────────────────
        if (tok.peek().is(TokenKind::Implement)) {
            tok.advance();

            // syntax: implement Type for Trait { ... }
            // or:      implement Trait for Type { ... }
            // during scan: skip to '{' and create unbody
            while (!tok.is_empty() && !tok.peek().is_eof()) {
                if (tok.peek().punc == '{') {
                    skipBalanced(tok, '{', '}');
                    break;
                }
                if (tok.peek().punc == ';') {
                    tok.advance();
                    break;
                }
                tok.advance();
            }

            current_vis       = import::SymbolVisibility::Private;
            current_mod_depth = 0;
            lastDocSpan       = {};
            continue;
        }

        // ── import declaration (from / import / export) ─────────────
        if (tok.peek().is(TokenKind::Module)) {
            auto kw_span = tok.peek().span;
            auto kw      = tok.lexeme();
            tok.advance();

            auto parse_path = [&](memory::DynArray<std::string_view> &p) {
                while (!tok.is_empty() && !tok.peek().is_eof()) {
                    if (tok.peek().is(TokenKind::Identifier)) {
                        p.push(tok.lexeme());
                        tok.advance();
                    } else if (tok.peek().punc == '/') {
                        tok.advance();
                    } else if (tok.peek().is(TokenKind::Punctuation) && tok.peek().punc == '.') {
                        if (tok.peek(1).is(TokenKind::Punctuation) && tok.peek(1).punc == '.') {
                            p.push(std::string_view{"..", 2});
                            tok.advance(2);
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            };

            auto parse_depth = [&]() -> int32_t {
                if (tok.peek().punc == '(') {
                    tok.advance();
                    if (tok.peek().punc == '.') {
                        tok.advance();
                        tok.advance();
                        if (tok.peek().punc == ')')
                            tok.advance();
                        return -1;
                    } else if (tok.peek().is(TokenKind::LitVal)) {
                        auto n = tok.lexeme();
                        tok.advance();
                        char *end = nullptr;
                        long val  = std::strtol(n.data(), &end, 10);
                        if (end == n.data() || val <= 0 || val > INT32_MAX) {
                            diag.report(Severity::Error, InvalidImportDepth,
                                        "import depth must be a positive integer or '..'",
                                        tok.peek().span);
                            val = 1;
                        }
                        if (tok.peek().punc == ')')
                            tok.advance();
                        return static_cast<int32_t>(val);
                    } else {
                        diag.report(Severity::Error, InvalidImportDepth,
                                    "expected import depth: positive integer or '..'",
                                    tok.peek().span);
                        if (tok.peek().punc == ')')
                            tok.advance();
                        return 1;
                    }
                }
                return 1;
            };

            if (kw == "from" || kw == "export") {
                memory::DynArray<std::string_view> path{bld.arena()};
                parse_path(path);
                if (path.empty()) {
                    diag.report(Severity::Error, ImportError,
                                "expected import path after '" + std::string(kw) + "'",
                                tok.peek().span);
                } else {
                    auto import_depth = parse_depth();
                    auto decl = bld.importDecl(std::move(path), {}, kw == "from" || kw == "export",
                                               kw == "export", import_depth, kw_span);
                    program.decls.push(decl);
                }
            } else if (kw == "import") {
                memory::DynArray<std::string_view> path{bld.arena()};
                parse_path(path);
                if (path.empty()) {
                    diag.report(Severity::Error, ImportError, "expected import path after 'import'",
                                tok.peek().span);
                } else {
                    auto import_depth = parse_depth();
                    std::string_view alias{};
                    if (tok.peek().is(TokenKind::As)) {
                        tok.advance();
                        if (tok.peek().is(TokenKind::Identifier)) {
                            alias = tok.lexeme();
                            tok.advance();
                        }
                    }
                    auto decl = bld.importDecl(std::move(path), alias, false, false, import_depth,
                                               kw_span);
                    program.decls.push(decl);
                }
            }
            current_vis       = import::SymbolVisibility::Private;
            current_mod_depth = 0;
            lastDocSpan       = {};
            continue;
        }

        // ── unknown token → skip ───────────────────────────────────
        diag.report(Severity::Error, ExpectedExpr, "unexpected token", tok.peek().span);
        tok.advance();
        lastDocSpan       = {};
        current_vis       = import::SymbolVisibility::Private;
        current_mod_depth = 0;
    }

    return result;
}

// ── type expression parsing ────────────────────────────────────────────

namespace {

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
            // qualifier without pointer and without '*' — already handled above
            // but if we consumed mut and there's no ownership/*, it's an error
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

// ── struct body expansion (parse field list) ─────────────────────────

namespace {

struct FieldInfo {
    std::string_view name;
    std::string_view type_lexeme;
};

void parseStructBody(Parser &p, memory::Span body_span, memory::DynArray<FieldInfo> &out_fields) {
    auto *tok  = p.tok;
    auto *diag = p.diag;

    // advance past '{'
    if (tok->peek().punc == '{')
        tok->advance();

    while (!tok->is_empty() && !tok->peek().is_eof()) {
        if (tok->peek().punc == '}')
            break;

        // visibility modifier
        if (tok->peek().is(lexer::TokenKind::Visibility)) {
            tok->advance();
            if (tok->peek().punc == '(' && tok->lexeme() == "mod")
                while (!tok->is_empty() && tok->peek().punc != ')')
                    tok->advance();
            if (tok->peek().punc == ')')
                tok->advance();
            continue;
        }

        // method fn — skip to ';' or body
        if (tok->peek().is(lexer::TokenKind::Fn) ||
            (tok->peek().is(lexer::TokenKind::Control) && tok->lexeme() == "flowing")) {
            while (!tok->is_empty() && !tok->peek().is_eof()) {
                if (tok->peek().punc == ';') {
                    tok->advance();
                    break;
                }
                if (tok->peek().punc == '{') {
                    skipBalanced(*tok, '{', '}');
                    break;
                }
                if (tok->peek().punc == '}')
                    break;
                tok->advance();
            }
            continue;
        }

        // destructure: [a, b, c]: Type
        if (tok->peek().punc == '[') {
            tok->advance();
            while (!tok->is_empty() && tok->peek().punc != ']') {
                if (tok->peek().is(lexer::TokenKind::Identifier)) {
                    auto fname = tok->lexeme();
                    tok->advance();
                    out_fields.push({fname, {}});
                } else {
                    tok->advance();
                }
                if (tok->peek().punc == ',')
                    tok->advance();
            }
            if (tok->peek().punc == ']')
                tok->advance();
            if (tok->peek().punc == ':') {
                tok->advance();
                skipTypeExpr(*tok);
            }
            if (tok->peek().punc == '=') {
                tok->advance();
                skipExpr(*tok);
            }
            if (tok->peek().punc == ',')
                tok->advance();
            continue;
        }

        // field: ident : Type = default ,
        if (tok->peek().is(lexer::TokenKind::Identifier)) {
            auto fname = tok->lexeme();
            tok->advance();
            std::string_view type_lexeme;
            if (tok->peek().punc == ':') {
                tok->advance();
                auto type_start = tok->offset;
                skipTypeExpr(*tok);
                type_lexeme = tok->lexeme(); // won't be correct for multi-token types
            }
            out_fields.push({fname, type_lexeme});
            if (tok->peek().punc == '=') {
                tok->advance();
                skipExpr(*tok);
            }
            if (tok->peek().punc == ',')
                tok->advance();
            continue;
        }

        tok->advance();
    }

    if (tok->peek().punc == '}')
        tok->advance();
}

} // anonymous namespace

// ── Parser helper implementations ───────────────────────────────────────

ast::TypeExprId Parser::parseOptTypeAnnotation() {
    if (peek().punc == ':') {
        advance();
        return parseTypeExpr();
    }
    return ast::kInvalidTypeExpr;
}

void Parser::skipUntil(std::initializer_list<lexer::TokenKind> sync_tokens) {
    recovery::panic(*tok, sync_tokens);
}

// ── expand bodies (second pass: seek + parse real bodies) ──────────────

void Parser::expandBodies(ScanResult &result) {
    for (auto &entry : result.fns) {
        if (entry.body_node == kInvalidExpr)
            continue;

        // Re-parse fn signature for typed params and return type
        tok->offset = entry.param_token_start;

        memory::DynArray<ast::FnParam> typed_params{bld->arena()};
        while (!tok->is_empty()) {
            if (tok->peek().is_eof() || tok->peek().punc == ')')
                break;

            if (tok->peek().punc == '[') {
                // destructured params: [a, b, c]: Type
                tok->advance();
                memory::DynArray<std::string_view> names{bld->arena()};
                while (!tok->is_empty() && tok->peek().punc != ']') {
                    if (tok->peek().is(TokenKind::Identifier)) {
                        names.push(tok->lexeme());
                        tok->advance();
                    } else {
                        tok->advance();
                    }
                    if (tok->peek().punc == ',')
                        tok->advance();
                }
                if (tok->peek().punc == ']')
                    tok->advance();
                ast::TypeExprId type_expr = parseOptTypeAnnotation();
                for (auto &n : names)
                    typed_params.push({n, type_expr});
            } else if (tok->peek().is(TokenKind::Identifier)) {
                auto name = tok->lexeme();
                tok->advance();
                ast::TypeExprId type_expr = parseOptTypeAnnotation();
                typed_params.push({name, type_expr});
            } else {
                tok->advance();
            }

            if (tok->peek().punc == ',')
                tok->advance();
        }
        consume(')');
        ast::TypeExprId return_type = parseOptTypeAnnotation();

        // Find the corresponding FnDeclNode and update its params/return_type
        for (auto &decl_id : program.decls) {
            auto &decl = bld->getDecl(decl_id);
            if (auto *fn = std::get_if<ast::FnDeclNode>(&decl)) {
                if (fn->body == entry.body_node) {
                    fn->params = std::move(typed_params);
                    fn->return_type = return_type;
                    break;
                }
            }
        }

        // Now parse the body — position should be at `{`
        auto &unbody = std::get<ast::UnbodyNode>(bld->getExpr(entry.body_node));
        tok->offset = unbody.token_start;

        consume('{');
        auto body_id = parseBlock();

        bld->getExpr(entry.body_node) = std::move(bld->getExpr(body_id));
    }

    memory::Arena field_arena;
    memory::DynArray<FieldInfo> fields_tmp{field_arena};

    for (auto &entry : result.structs) {
        if (entry.body_node == kInvalidExpr)
            continue;

        auto &unbody = std::get<ast::UnbodyNode>(bld->getExpr(entry.body_node));

        tok->offset = unbody.token_start;

        fields_tmp.clear();
        parseStructBody(*this, entry.span, fields_tmp);
    }

    // enum/union/trait body expansion deferred — structure is already scanned
    // result.enums, result.unions, result.traits entries exist with body spans
}

// ── public API (backward compat) ─────────────────────────────────────

ProgramResult parseProgram(lexer::TokenStream tokens, ast::AstBuilder &builder,
                           diagnostics::DiagnosticEngine &diags) {
    memory::Arena arena;
    import::SymbolTable syms(arena);
    Parser p(&tokens, &builder, &diags);
    auto result = scan(p, syms);
    p.expandBodies(result);
    return std::move(p.program);
}

} // namespace zith::parser
