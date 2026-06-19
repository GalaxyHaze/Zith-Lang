#include "parser.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-table.hpp"
#include "lexer/lexer.hpp"
#include "memory/source-map.hpp"
#include "parser/recovery.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace zith::parser {

namespace {

using ast::kInvalidDecl;
using ast::kInvalidExpr;
using ast::kInvalidStmt;
using diagnostics::Severity;
using lexer::TokenKind;
using namespace zith::diagnostics::err;

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

// ── operator precedence ────────────────────────────────────────────────

namespace {

// Pratt infix precedence by operator token
uint8_t infixPrec(const lexer::Token &t) {
    if (t.is(lexer::TokenKind::Punctuation)) {
        switch (t.punc) {
        case '|': return 6;
        case '^': return 7;
        case '&': return 8;
        case '<': return 10;
        case '>': return 10;
        case '+': return 12;
        case '-': return 12;
        case '*': return 14;
        case '/': return 14;
        case '%': return 14;
        default:  return 0;
        }
    }
    if (t.is(lexer::TokenKind::Logical)) {
        // 'and' = 4, 'or' = 2, 'xor' = 5
        return 2; // will refine per token
    }
    if (t.is(lexer::TokenKind::Operators)) {
        switch (t.punc) {
        case '|': return 6;
        case '^': return 7;
        case '&': return 8;
        case '<': return 10;
        case '>': return 10;
        case '+': return 12;
        case '-': return 12;
        case '*': return 14;
        case '/': return 14;
        case '%': return 14;
        case '=': return 0;  // '=' is assignment, not binary comparison
        case '!': return 0;  // '!' is prefix/postfix, not binary
        case '~': return 0;
        case '?': return 0;
        default:  return 0;
        }
    }
    return 0;
}

// Word operator ('and', 'or', 'xor') → BinaryOp
ast::BinaryOp binaryOpForWord(std::string_view w) {
    if (w == "and") return ast::BinaryOp::And;
    if (w == "or")  return ast::BinaryOp::Or;
    if (w == "xor") return ast::BinaryOp::Xor;
    return ast::BinaryOp::Add; // unreachable
}

uint8_t wordPrec(std::string_view w) {
    if (w == "or")  return 2;
    if (w == "xor") return 4;
    if (w == "and") return 3;
    return 0;
}

// Map a multi-char operator sequence to a BinaryOp.
// Returns true if the two tokens form a compound operator.
bool tryCompoundOp(const lexer::Token &first, const lexer::Token &second,
                   ast::BinaryOp &out_op) {
    if (!first.is(lexer::TokenKind::Operators) || !second.is(lexer::TokenKind::Operators))
        return false;
    if (first.punc == '=' && second.punc == '=') { out_op = ast::BinaryOp::Eq; return true; }
    if (first.punc == '!' && second.punc == '=') { out_op = ast::BinaryOp::Ne; return true; }
    if (first.punc == '<' && second.punc == '=') { out_op = ast::BinaryOp::Le; return true; }
    if (first.punc == '>' && second.punc == '=') { out_op = ast::BinaryOp::Ge; return true; }
    if (first.punc == '<' && second.punc == '<') { out_op = ast::BinaryOp::Shl; return true; }
    if (first.punc == '>' && second.punc == '>') { out_op = ast::BinaryOp::Shr; return true; }
    if (first.punc == '=' && second.punc == '>') { out_op = ast::BinaryOp::Eq; return true; }
    // && and || are not valid — use and / or instead
    return false;
}

// Single-char operator → BinaryOp
ast::BinaryOp binaryOpForChar(char c) {
    switch (c) {
    case '+': return ast::BinaryOp::Add;
    case '-': return ast::BinaryOp::Sub;
    case '*': return ast::BinaryOp::Mul;
    case '/': return ast::BinaryOp::Div;
    case '%': return ast::BinaryOp::Rest;
    case '<': return ast::BinaryOp::Lt;
    case '>': return ast::BinaryOp::Gt;
    case '=': return ast::BinaryOp::Eq;   // single '=' is not Eq in parser — only == is
    case '!': return ast::BinaryOp::Ne;   // single '!' is not Ne — it's prefix/postfix
    case '&': return ast::BinaryOp::And;  // single '&' is not And in parser
    case '|': return ast::BinaryOp::Or;   // single '|' is not Or in parser
    default:  return ast::BinaryOp::Add;  // unreachable
    }
}

} // anonymous namespace

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
        advance();
        auto cond = parseExpr();
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
        return bld->ifExpr(cond, then_branch, else_branch);
    }

    // 'while' expression
    if (peek().is(TokenKind::Control) && lexeme() == "while") {
        advance();
        auto cond = parseExpr();
        ast::ExprId body = kInvalidExpr;
        if (peek().punc == '{') {
            advance();
            body = parseBlock();
        }
        return bld->whileExpr(cond, body);
    }

    // 'for' expression — desugar into while
    if (peek().is(TokenKind::For) && lexeme() == "for") {
        advance();
        // for (init; cond; step) body
        // desugar: { init; while cond { body; step; } }
        auto desugar_block = bld->block(memory::DynArray<ast::StmtId>{bld->arena()});
        auto &block = std::get<ast::BlockNode>(bld->getExpr(desugar_block));

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
            auto &bb = std::get<ast::BlockNode>(bld->getExpr(body_block));
            if (loop_body != kInvalidExpr) {
                auto &lb = std::get<ast::BlockNode>(bld->getExpr(loop_body));
                for (auto s : lb.stmts)
                    bb.stmts.push(s);
                bb.trailing = lb.trailing;
            }
            if (step != kInvalidExpr)
                bb.stmts.push(bld->addStmt(step));

            ast::ExprId while_cond = cond != kInvalidExpr ? cond : bld->litExpr(ast::LitKind::Bool, "true");
            auto while_expr = bld->whileExpr(while_cond, body_block);
            block.stmts.push(bld->addStmt(while_expr));
        }

        return desugar_block;
    }

    if (peek().is(TokenKind::LitVal)) {
        auto lit = lexeme();
        advance();
        auto kind = ast::LitKind::Int;
        if (lit == "true" || lit == "false")
            kind = ast::LitKind::Bool;
        else if (lit == "null")
            kind = ast::LitKind::Nil;
        return bld->litExpr(kind, lit);
    }

    if (peek().is(TokenKind::Identifier)) {
        auto name = lexeme();
        advance();
        return bld->ident(name);
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
        advance();
        return bld->unary(ast::UnaryOp::Neg, parsePrefix());
    }
    if (peek().punc == '!') {
        advance();
        return bld->unary(ast::UnaryOp::Not, parsePrefix());
    }
    if (peek().punc == '&') {
        advance();
        return bld->unary(ast::UnaryOp::Ref, parsePrefix());
    }
    if (peek().punc == '*') {
        advance();
        return bld->unary(ast::UnaryOp::Deref, parsePrefix());
    }
    // word prefix 'not'
    if (peek().is(TokenKind::Logical) && lexeme() == "not") {
        advance();
        return bld->unary(ast::UnaryOp::Not, parsePrefix());
    }

    return parsePrimary();
}

ast::ExprId Parser::parseExpr(int min_prec) {
    auto lhs = parsePrefix();

    for (;;) {
        if (eof()) break;

        auto &tok = peek();

        // ── Postfix: call ( args ) ──────────────────────────────
        if (tok.punc == '(') {
            advance();
            memory::DynArray<ast::ExprId> args{bld->arena()};
            if (peek().punc != ')') {
                args.push(parseExpr());
                while (consume(','))
                    args.push(parseExpr());
            }
            if (!consume(')'))
                diag->report(Severity::Error, UnclosedParen, "expected ')'", peek().span);
            lhs = bld->call(lhs, std::move(args));
            continue;
        }

        // ── Postfix: ..range ────────────────────────────────────
        if (tok.punc == '.' && peek(1).punc == '.') {
            advance(2);
            auto rhs = parseExpr(min_prec + 1);
            lhs = bld->range(lhs, rhs);
            continue;
        }

        // ── Postfix: .field ─────────────────────────────────────
        if (tok.punc == '.') {
            advance();
            if (!peek().is(TokenKind::Identifier)) {
                diag->report(Severity::Error, ExpectedIdent, "expected field name after '.'",
                             peek().span);
                continue;
            }
            auto field = lexeme();
            advance();
            lhs = bld->field(lhs, field);
            continue;
        }

        // ── Postfix: ->field (member access by pointer) ─────────
        if (tok.is(lexer::TokenKind::Operators) && tok.punc == '-' && peek(1).punc == '>') {
            advance(2);
            if (!peek().is(TokenKind::Identifier)) {
                diag->report(Severity::Error, ExpectedIdent, "expected field name after '->'",
                             peek().span);
                continue;
            }
            auto field = lexeme();
            advance();
            lhs = bld->field(lhs, field);
            continue;
        }

        // ── Postfix: [index] ────────────────────────────────────
        if (tok.punc == '[') {
            advance();
            auto index = parseExpr();
            if (!consume(']'))
                diag->report(Severity::Error, ExpectedExpr, "expected ']'", peek().span);
            lhs = bld->index(lhs, index);
            continue;
        }

        // ── Postfix: unwrap ! ───────────────────────────────────
        if (tok.is(lexer::TokenKind::Operators) && tok.punc == '!' &&
            !peek(1).is(lexer::TokenKind::Operators)) {
            // single '!' = unwrap postfix; '!=' is handled as compound op
            advance();
            // wrap in unary "deref" as unwrap placeholder
            lhs = bld->unary(ast::UnaryOp::Deref, lhs);
            continue;
        }

        // ── Word infix: and / or / xor ──────────────────────────
        if (tok.is(lexer::TokenKind::Logical)) {
            auto word = lexeme();
            uint8_t prec = wordPrec(word);
            if (prec == 0 || prec < min_prec) break;
            advance();
            auto rhs = parseExpr(prec + 1);
            lhs = bld->binary(lhs, binaryOpForWord(word), rhs);
            continue;
        }

        // ── Compound operator: ==, !=, <=, >=, <<, >> ───
        ast::BinaryOp compound_op;
        if (tryCompoundOp(tok, peek(1), compound_op)) {
            uint8_t prec = infixPrec(tok);
            if (prec < min_prec) break;
            advance(2);
            auto rhs = parseExpr(prec + 1);
            lhs = bld->binary(lhs, compound_op, rhs);
            continue;
        }

        // ── Infix binary operator (single char) ─────────────────
        uint8_t prec = infixPrec(tok);
        if (prec == 0 || prec < min_prec) break;

        // Check that it's really an infix operator, not part of a compound
        ast::BinaryOp compound_check;
        if (tryCompoundOp(tok, peek(1), compound_check)) {
            // let the compound handler above deal with it on next iteration
            break;
        }

        char op_punc = tok.punc;
        advance();
        auto rhs = parseExpr(prec + 1);
        lhs = bld->binary(lhs, binaryOpForChar(op_punc), rhs);
    }

    return lhs;
}

ast::ExprId Parser::parseExpr() {
    return parseExpr(0);
}

ast::ExprId Parser::parseBlock() {
    memory::DynArray<ast::StmtId> stmts{bld->arena()};

    while (!eof()) {
        skipComments(*tok);
        if (peek().punc == '}')
            break;
        stmts.push(parseStmt());
    }

    consume('}');
    return bld->block(std::move(stmts));
}

// ── statement parsing ──────────────────────────────────────────────────

ast::StmtId Parser::parseStmt() {
    skipComments(*tok);
    if (peek().is(TokenKind::Control) && lexeme() == "return") {
        advance();
        auto val = eof() || peek().punc == '}' ? kInvalidExpr : parseExpr();
        if (peek().punc == ';')
            advance();
        else
            recovery::panic(*tok, {TokenKind::End, TokenKind::Punctuation});
        return bld->retStmt(val);
    }

    if (peek().is(TokenKind::Control) && lexeme() == "break") {
        advance();
        if (peek().punc == ';') advance();
        // placeholder: break with no loop context
        return bld->addStmt(bld->litExpr(ast::LitKind::Nil, "null"));
    }

    if (peek().is(TokenKind::Control) && lexeme() == "continue") {
        advance();
        if (peek().punc == ';') advance();
        return bld->addStmt(bld->litExpr(ast::LitKind::Nil, "null"));
    }

    if (peek().is(TokenKind::Variable)) {
        advance();
        if (!peek().is(TokenKind::Identifier)) {
            diag->report(Severity::Error, ExpectedIdent, "expected variable name", peek().span);
            recovery::panic(*tok, {TokenKind::End, TokenKind::Punctuation});
            return kInvalidStmt;
        }
        auto name = lexeme();
        advance();
        ast::ExprId init = kInvalidExpr;
        if (consume('='))
            init = parseExpr();
        if (peek().punc == ';')
            advance();
        return bld->letStmt(name, false, init);
    }

    auto expr = parseExpr();
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
    auto name = lexeme();
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

    return bld->fnDecl(name, std::move(params), body);
}

// ── scan (first pass: register symbols, skip bodies) ───────────────────

namespace {

[[nodiscard]] memory::Span span_from_offset(uint32_t start, uint32_t end) {
    return {0, start, end};
}

uint32_t skip_body_tokens(lexer::TokenStream &tok) {
    if (tok.is_empty())
        return tok.offset;
    uint32_t depth = 1;
    tok.advance();
    while (!tok.is_empty() && depth > 0) {
        if (tok.peek().kind == TokenKind::End)
            break;
        if (tok.peek().punc == '{')
            depth++;
        else if (tok.peek().punc == '}')
            depth--;
        tok.advance();
    }
    return tok.offset;
}

static const char *symKindName(import::SymKind k) {
    switch (k) {
    case import::SymKind::Fn:
        return "a fn";
    case import::SymKind::Struct:
        return "a struct";
    case import::SymKind::Trait:
        return "a trait";
    case import::SymKind::Interface:
        return "a interface";
    case import::SymKind::Enum:
        return "an enum";
    case import::SymKind::Alias:
        return "an alias";
    case import::SymKind::Variable:
        return "a variable";
    case import::SymKind::Module:
        return "a module";
    case import::SymKind::Component:
        return "a component";
    case import::SymKind::Union:
        return "a union";
    }
    return "unknown";
}

// skip past a balanced pair: ( ... ), { ... }, [ ... ]
// cursor must be ON the opening delimiter; it is consumed first.
static void skip_balanced(lexer::TokenStream &tok, char open, char close) {
    int depth = 1;
    tok.advance(); // consume the opening delimiter
    while (!tok.is_empty()) {
        if (tok.peek().is_eof())
            break;
        if (tok.peek().punc == open)
            depth++;
        else if (tok.peek().punc == close)
            depth--;
        tok.advance();
        if (depth == 0)
            break;
    }
}

// skip past a type expression during scan — just advances past tokens until a
// terminator (comma, brace, bracket, semicolon, equals, paren) is found.
static void scan_skip_type_expr(lexer::TokenStream &tok) {
    while (!tok.is_empty()) {
        auto &t = tok.peek();
        if (t.is_eof())
            break;
        if (t.punc == ',' || t.punc == '}' || t.punc == ']' || t.punc == ';' || t.punc == '=' ||
            t.punc == ')' || t.punc == '|')
            break;
        if (t.punc == '(') {
            skip_balanced(tok, '(', ')');
            continue;
        }
        if (t.punc == '|') {
            skip_balanced(tok, '|', '|');
            continue;
        }
        if (t.punc == '[') {
            skip_balanced(tok, '[', ']');
            continue;
        }
        tok.advance();
    }
}

// skip past an expression during scan until a terminator
static void scan_skip_expr(lexer::TokenStream &tok) {
    while (!tok.is_empty()) {
        auto &t = tok.peek();
        if (t.is_eof())
            break;
        if (t.punc == ',' || t.punc == '}' || t.punc == ';' || t.punc == ')')
            break;
        if (t.punc == '(') {
            skip_balanced(tok, '(', ')');
            continue;
        }
        if (t.punc == '{') {
            skip_balanced(tok, '{', '}');
            continue;
        }
        if (t.punc == '[') {
            skip_balanced(tok, '[', ']');
            continue;
        }
        tok.advance();
    }
}

[[nodiscard]] bool reportIfDuplicate(import::SymbolTable &syms, diagnostics::DiagnosticEngine &diag,
                                     std::string_view name, memory::Span span,
                                     import::SymId skip_id = import::kInvalidSym) {
    auto existing = syms.lookup(name);
    if (existing == import::kInvalidSym || existing == skip_id)
        return false;
    auto &prev      = syms.get(existing);
    std::string msg = "duplicate symbol '";
    msg += name;
    msg += "' — previous declaration is ";
    msg += symKindName(prev.kind);
    diag.report(Severity::Error, DuplicateDecl, std::move(msg), span);
    return true;
}

} // anonymous namespace

ScanResult scan(Parser &parser, import::SymbolTable &syms) {
    auto &tok     = *parser.tok;
    auto &bld     = *parser.bld;
    auto &diag    = *parser.diag;
    auto &program = parser.program;

    ScanResult result{bld.arena()};
    import::SymbolVisibility current_vis = import::SymbolVisibility::Private;
    int32_t current_mod_depth            = 0;

    while (!tok.is_empty()) {
        if (tok.peek().is_eof())
            break;

        // ── skip comment / doc tokens ──────────────────────────────
        if (tok.peek().is(TokenKind::Comments) || tok.peek().is(TokenKind::Docs)) {
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
                        char *end = nullptr;
                        long val = std::strtol(n.data(), &end, 10);
                        current_mod_depth = (end > n.data() && val >= 1 && val <= INT32_MAX)
                            ? static_cast<int32_t>(val) : 1;
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

            memory::DynArray<std::string_view> params{bld.arena()};
            memory::DynArray<memory::Span> param_spans{bld.arena()};
            while (!tok.is_empty()) {
                if (tok.peek().is_eof())
                    break;
                if (tok.peek().punc == ')')
                    break;

                if (!tok.peek().is(TokenKind::Identifier)) {
                    diag.report(Severity::Error, ExpectedIdent, "expected parameter name",
                                tok.peek().span);
                    tok.advance();
                    continue;
                }
                param_spans.push(tok.peek().span);
                params.push(tok.lexeme());
                tok.advance();

                if (tok.peek().punc == ',')
                    tok.advance();
            }
            if (tok.peek().punc == ')')
                tok.advance();

            ast::ExprId body_node = kInvalidExpr;
            uint32_t token_start  = 0;
            uint32_t token_end    = 0;
            memory::Span body_span{};

            if (tok.peek().punc == '{') {
                token_start = tok.offset;
                body_span   = tok.peek().span;
                token_end   = skip_body_tokens(tok);
                body_span   = {body_span.file, body_span.start, token_end};
                body_node   = bld.unbody(body_span, token_start, token_end);
            }

            auto fn_sym = import::kInvalidSym;
            if (!reportIfDuplicate(syms, diag, name, name_span)) {
                fn_sym = syms.declare(name, current_vis, current_mod_depth, import::SymKind::Fn,
                                      ast::kInvalidDecl, name_span);
                for (size_t i = 0; i < params.size(); i++) {
                    if (!reportIfDuplicate(syms, diag, params[i], param_spans[i], fn_sym)) {
                        auto ps = syms.declare(params[i], current_vis, current_mod_depth,
                                               import::SymKind::Variable);
                        syms.get(fn_sym).members.push(ps);
                    }
                }
            }

            auto decl = bld.fnDecl(name, std::move(params), body_node);
            program.decls.push(decl);
            if (fn_sym != import::kInvalidSym)
                syms.get(fn_sym).decl_id = decl;

            result.fns.push({name, body_span, body_node});
            current_vis       = import::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── struct-like declaration (struct / enum / union / component) ─
        if (tok.peek().is(TokenKind::Struct)) {
            auto kw   = tok.lexeme();
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

            auto decl = ast::kInvalidDecl;
            ast::ExprId body_node = kInvalidExpr;
            memory::Span body_span{};

            // create appropriate AST node
            if (kw == "struct") {
                decl = bld.structDecl(
                    name,
                    memory::DynArray<ast::StructField>(bld.arena()));
            } else if (kw == "enum") {
                decl = bld.enumDecl(
                    name,
                    memory::DynArray<ast::EnumVariant>(bld.arena()));
            } else if (kw == "union") {
                decl = bld.unionDecl(
                    name,
                    memory::DynArray<ast::UnionVariant>(bld.arena()));
            } else {
                decl = bld.componentDecl(name);
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
                                skip_balanced(tok, '(', ')');
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
                                                           import::SymKind::Fn);
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
                                    skip_balanced(tok, '{', '}');
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
                                        auto fs = syms.declare(
                                            fname, import::SymbolVisibility::Private, 0,
                                            import::SymKind::Variable);
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
                                scan_skip_type_expr(tok);
                            }
                            // skip default value
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_skip_expr(tok);
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
                                                       import::SymKind::Variable);
                                if (decl != ast::kInvalidDecl && fs != import::kInvalidSym)
                                    syms.get(fs).decl_id = decl;
                            }
                            // skip type annotation
                            if (tok.peek().punc == ':') {
                                tok.advance();
                                scan_skip_type_expr(tok);
                            }
                            // skip default value
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_skip_expr(tok);
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
                                                       import::SymKind::Variable);
                                if (decl != ast::kInvalidDecl && vs != import::kInvalidSym)
                                    syms.get(vs).decl_id = decl;
                            }

                            // tuple variant: Variant(Type1, Type2)
                            if (tok.peek().punc == '(')
                                skip_balanced(tok, '(', ')');
                            // struct variant: Variant { field: Type }
                            if (tok.peek().punc == '{')
                                skip_balanced(tok, '{', '}');
                            // discriminant assignment: Variant = 1
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_skip_expr(tok);
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
                                                       import::SymKind::Variable);
                                if (decl != ast::kInvalidDecl && vs != import::kInvalidSym)
                                    syms.get(vs).decl_id = decl;
                            }

                            // skip type annotation: variant: Type
                            if (tok.peek().punc == ':') {
                                tok.advance();
                                scan_skip_type_expr(tok);
                            }
                            // skip default
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_skip_expr(tok);
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
                    skip_balanced(tok, '{', '}');
                }

                body_span = {body_span.file, body_span.start, tok.offset};
                body_node = bld.unbody(body_span, token_start, tok.offset);
            }

            program.decls.push(decl);
            if (!reportIfDuplicate(syms, diag, name, name_span))
                syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span);

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
            auto kw   = tok.lexeme();
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

            import::SymKind sk = (kw == "interface")
                ? import::SymKind::Interface
                : import::SymKind::Trait;
            auto decl = (kw == "interface")
                ? bld.interfaceDecl(name, memory::DynArray<ast::TraitMethod>(bld.arena()))
                : bld.traitDecl(name, memory::DynArray<ast::TraitMethod>(bld.arena()));

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
                                                       import::SymKind::Fn);
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
                                skip_balanced(tok, '{', '}');
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
                syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span);

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
                    skip_balanced(tok, '{', '}');
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
            continue;
        }

        // ── import declaration (from / import / export) ─────────────
        if (tok.peek().is(TokenKind::Module)) {
            auto kw = tok.lexeme();
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
                                               kw == "export", import_depth);
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
                    auto decl = bld.importDecl(std::move(path), alias, false, false, import_depth);
                    program.decls.push(decl);
                }
            }
            current_vis       = import::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── unknown token → skip ───────────────────────────────────
        diag.report(Severity::Error, ExpectedExpr, "unexpected token", tok.peek().span);
        tok.advance();
        current_vis       = import::SymbolVisibility::Private;
        current_mod_depth = 0;
    }

    return result;
}

// ── struct body expansion (parse field list) ─────────────────────────

namespace {

struct FieldInfo {
    std::string_view name;
    std::string_view type_lexeme;
};

void parseStructBody(Parser &p, memory::Span body_span, memory::DynArray<FieldInfo> &out_fields) {
    auto *tok = p.tok;
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
                while (!tok->is_empty() && tok->peek().punc != ')') tok->advance();
            if (tok->peek().punc == ')') tok->advance();
            continue;
        }

        // method fn — skip to ';' or body
        if (tok->peek().is(lexer::TokenKind::Fn) ||
            (tok->peek().is(lexer::TokenKind::Control) && tok->lexeme() == "flowing")) {
            while (!tok->is_empty() && !tok->peek().is_eof()) {
                if (tok->peek().punc == ';') { tok->advance(); break; }
                if (tok->peek().punc == '{') { skip_balanced(*tok, '{', '}'); break; }
                if (tok->peek().punc == '}') break;
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
                } else { tok->advance(); }
                if (tok->peek().punc == ',') tok->advance();
            }
            if (tok->peek().punc == ']') tok->advance();
            if (tok->peek().punc == ':') { tok->advance(); scan_skip_type_expr(*tok); }
            if (tok->peek().punc == '=') { tok->advance(); scan_skip_expr(*tok); }
            if (tok->peek().punc == ',') tok->advance();
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
                scan_skip_type_expr(*tok);
                type_lexeme = tok->lexeme();  // won't be correct for multi-token types
            }
            out_fields.push({fname, type_lexeme});
            if (tok->peek().punc == '=') { tok->advance(); scan_skip_expr(*tok); }
            if (tok->peek().punc == ',') tok->advance();
            continue;
        }

        tok->advance();
    }

    if (tok->peek().punc == '}')
        tok->advance();
}

} // anonymous namespace

// ── expand bodies (second pass: seek + parse real bodies) ──────────────

void Parser::expandBodies(ScanResult &result) {
    for (auto &entry : result.fns) {
        if (entry.body_node == kInvalidExpr)
            continue;

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
