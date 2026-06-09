#include "parser.hpp"
#include "memory/source-map.hpp"
#include "lexer/lexer.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-table.hpp"

#include <string_view>
#include <vector>

namespace zith::parser {
    
namespace {

    using lexer::TokenKind;
    using ast::kInvalidDecl;
    using ast::kInvalidExpr;
    using ast::kInvalidStmt;
    using diagnostics::Severity;
    using namespace zith::diagnostics::err;

} // anonymous namespace

    // ── token helpers ──────────────────────────────────────────────────────

    std::string_view Parser::lexeme() {
        auto res = memory::SourceMap::snippet(tok->peek().span);
        return res.isOk() ? res.value() : std::string_view{};
    }

    const lexer::Token &Parser::peek() { return tok->peek(); }
    const lexer::Token &Parser::peek(uint32_t n) { return tok->peek(n); }
    void Parser::advance() { tok->advance(); }
    void Parser::advance(uint32_t n) { tok->advance(n); }
    bool Parser::eof() { return peek().is_eof(); }

    bool Parser::match(TokenKind kind) {
        return tok->match(kind);
    }

    bool Parser::expect(TokenKind kind) {
        if (match(kind)) return true;
        diag->report(Severity::Error, ExpectedExpr,
                       std::string("expected ") + lexer::tokenKindName(kind) +
                       " but got " + lexer::tokenKindName(peek().kind),
                       peek().span);
        return false;
    }

    bool Parser::consume(char c) {
        auto l = lexeme();
        if (l.size() == 1 && l[0] == c) {
            advance();
            return true;
        }
        return false;
    }

    // ── expression parsing ─────────────────────────────────────────────────

    ast::ExprId Parser::parsePrimary() {
        if (consume('{'))
            return parseBlock();

        if (peek().is(TokenKind::LitVal)) {
            auto lit = lexeme();
            advance();
            return bld->litExpr(ast::LitKind::Int, lit);
        }

        if (peek().is(TokenKind::Identifier)) {
            auto name = lexeme();
            advance();
            return bld->ident(name);
        }

        if (peek().is(TokenKind::Punctuation) && lexeme() == "(") {
            advance();
            auto inner = parseExpr();
            if (!consume(')'))
                diag->report(Severity::Error, UnclosedParen,
                               "expected ')'", peek().span);
            return inner;
        }

        diag->report(Severity::Error, ExpectedExpr,
                       "expected expression", peek().span);
        advance();
        return kInvalidExpr;
    }

    ast::ExprId Parser::parseExpr(int min_prec) {
        auto lhs = parsePrimary();
        (void)min_prec;
        return lhs;
    }

    ast::ExprId Parser::parseExpr() {
        return parseExpr(0);
    }

    ast::ExprId Parser::parseBlock() {
        memory::DynArray<ast::StmtId> stmts{memory::SessionArena};

        while (!eof()) {
            if (peek().is(TokenKind::Punctuation) && lexeme() == "}")
                break;
            stmts.push(parseStmt());
        }

        consume('}');
        return bld->block(std::move(stmts));
    }

    // ── statement parsing ──────────────────────────────────────────────────

    ast::StmtId Parser::parseStmt() {
        if (peek().is(TokenKind::Control) && lexeme() == "return") {
            advance();
            auto val = eof() || (peek().is(TokenKind::Punctuation) && lexeme() == "}")
                           ? kInvalidExpr
                           : parseExpr();
            return bld->retStmt(val);
        }

        if (peek().is(TokenKind::Variable)) {
            advance();
            if (!peek().is(TokenKind::Identifier)) {
                diag->report(Severity::Error, ExpectedIdent,
                               "expected variable name", peek().span);
                advance();
                return kInvalidStmt;
            }
            auto name = lexeme();
            advance();
            ast::ExprId init = kInvalidExpr;
            if (consume('='))
                init = parseExpr();
            return bld->letStmt(name, false, init);
        }

        auto expr = parseExpr();
        return bld->addStmt(expr);
    }

    // ── declaration parsing ────────────────────────────────────────────────

    ast::DeclId Parser::parseFnDecl() {
        if (!peek().is(TokenKind::Identifier)) {
            diag->report(Severity::Error, ExpectedIdent,
                           "expected function name", peek().span);
            return kInvalidDecl;
        }
        auto name = lexeme();
        advance();

        if (!consume('(')) {
            diag->report(Severity::Error, ExpectedExpr,
                           "expected '(' after function name", peek().span);
            return kInvalidDecl;
        }

        memory::DynArray<std::string_view> params{memory::SessionArena};
        while (!eof() && !(peek().is(TokenKind::Punctuation) && lexeme() == ")")) {
            if (!peek().is(TokenKind::Identifier)) {
                diag->report(Severity::Error, ExpectedIdent,
                               "expected parameter name", peek().span);
                advance();
                continue;
            }
            params.push(lexeme());
            advance();
            if (lexeme() == ",")
                advance();
        }
        consume(')');

        ast::ExprId body = kInvalidExpr;
        if (peek().is(TokenKind::Punctuation) && lexeme() == "{")
            body = parseBlock();

        return bld->fnDecl(name, std::move(params), body);
    }

    // ── scan (first pass: register symbols, skip bodies) ───────────────────

    namespace {

        [[nodiscard]] memory::Span span_from_offset(uint32_t start, uint32_t end) {
            // Build a Span from token offsets (file_id set by caller).
            // For now, we use a zero-file placeholder — the caller overwrites it.
            return {0, start, end};
        }

        uint32_t skip_body_tokens(lexer::TokenStream &tok) {
            // Count braces to find the end of a function body.
            // Expects the cursor at '{', returns token offset of the matching '}'.
            if (tok.is_empty()) return tok.offset;
            uint32_t depth = 1;
            tok.advance(); // consume '{'
            while (!tok.is_empty() && depth > 0) {
                if (tok.peek().kind == TokenKind::End) break;
                // Check for brace using lexeme
                auto res = memory::SourceMap::snippet(tok.peek().span);
                if (res.isOk()) {
                    auto s = res.value();
                    if (s.size() == 1) {
                        if (s[0] == '{') depth++;
                        else if (s[0] == '}') depth--;
                    }
                }
                tok.advance();
            }
            return tok.offset;
        }

    } // anonymous namespace

    ScanResult scan(Parser &parser, import::SymbolTable &syms, bool sema) {
        (void)sema; // used in future for import-only resolution
        auto &tok = *parser.tok;
        auto &bld = *parser.bld;
        auto &diag = *parser.diag;
        auto &program = parser.program;

        ScanResult result{memory::SessionArena};

        while (!tok.is_empty()) {
            if (tok.peek().is_eof()) break;

            // ── fn declaration ─────────────────────────────────────────
            if (tok.peek().is(TokenKind::Fn)) {
                tok.advance(); // consume 'fn'

                if (!tok.peek().is(TokenKind::Identifier)) {
                    diag.report(Severity::Error, ExpectedIdent,
                                "expected function name", tok.peek().span);
                    tok.advance();
                    continue;
                }

                auto name = [&]{
                    auto res = memory::SourceMap::snippet(tok.peek().span);
                    return res.isOk() ? res.value() : std::string_view{};
                }();
                tok.advance();

                // expected '('
                if (!(tok.peek().is(TokenKind::Punctuation) &&
                      [&]{ auto r = memory::SourceMap::snippet(tok.peek().span);
                           return r.isOk() && r.value() == "("; }())) {
                    diag.report(Severity::Error, ExpectedExpr,
                                "expected '(' after function name", tok.peek().span);
                    continue;
                }
                tok.advance(); // consume '('

                // parse params
                memory::DynArray<std::string_view> params{memory::SessionArena};
                while (!tok.is_empty()) {
                    if (tok.peek().is_eof()) break;
                    if (tok.peek().is(TokenKind::Punctuation) &&
                        [&]{ auto r = memory::SourceMap::snippet(tok.peek().span);
                             return r.isOk() && r.value() == ")"; }())
                        break;

                    if (!tok.peek().is(TokenKind::Identifier)) {
                        diag.report(Severity::Error, ExpectedIdent,
                                    "expected parameter name", tok.peek().span);
                        tok.advance();
                        continue;
                    }
                    {
                        auto r = memory::SourceMap::snippet(tok.peek().span);
                        if (r.isOk()) params.push(r.value());
                    }
                    tok.advance();

                    // skip optional comma
                    if (tok.peek().is(TokenKind::Punctuation) &&
                        [&]{ auto r = memory::SourceMap::snippet(tok.peek().span);
                             return r.isOk() && r.value() == ","; }())
                        tok.advance();
                }
                // consume ')'
                if (tok.peek().is(TokenKind::Punctuation) &&
                    [&]{ auto r = memory::SourceMap::snippet(tok.peek().span);
                         return r.isOk() && r.value() == ")"; }())
                    tok.advance();

                // skip body
                ast::ExprId body_node = kInvalidExpr;
                uint32_t token_start = 0;
                uint32_t token_end = 0;
                memory::Span body_span{};

                if (tok.peek().is(TokenKind::Punctuation) &&
                    [&]{ auto r = memory::SourceMap::snippet(tok.peek().span);
                         return r.isOk() && r.value() == "{"; }()) {
                    token_start = tok.offset;
                    body_span = tok.peek().span;
                    token_end = skip_body_tokens(tok);
                    body_span = {body_span.file, body_span.start, token_end};
                    body_node = bld.unbody(body_span, token_start, token_end);
                }

                auto decl = bld.fnDecl(name, std::move(params), body_node);
                program.decls.push(decl);
                syms.declare(name);

                result.fns.push({name, body_span, body_node});
                continue;
            }

            // ── struct declaration ─────────────────────────────────────
            if (tok.peek().is(TokenKind::Struct)) {
                tok.advance(); // consume 'struct'

                if (!tok.peek().is(TokenKind::Identifier)) {
                    diag.report(Severity::Error, ExpectedIdent,
                                "expected struct name", tok.peek().span);
                    tok.advance();
                    continue;
                }

                auto name = [&]{
                    auto res = memory::SourceMap::snippet(tok.peek().span);
                    return res.isOk() ? res.value() : std::string_view{};
                }();
                tok.advance();

                syms.declare(name);

                // skip fields for now
                result.structs.push({name, {}, kInvalidExpr});
                continue;
            }

            // ── import declaration (stub) ───────────────────────────────
            if (tok.peek().is(TokenKind::Module)) {
                tok.advance();
                // skip import path tokens until we hit a newline/semicolon-like boundary
                while (!tok.is_empty() && !tok.peek().is_eof()) {
                    if (tok.peek().is(TokenKind::Punctuation) &&
                        [&]{ auto r = memory::SourceMap::snippet(tok.peek().span);
                             return r.isOk() && r.value() == ";"; }()) {
                        tok.advance();
                        break;
                    }
                    tok.advance();
                }
                continue;
            }

            // ── unknown token → skip ───────────────────────────────────
            diag.report(Severity::Error, ExpectedExpr,
                        "unexpected token", tok.peek().span);
            tok.advance();
        }

        return result;
    }

    // ── expand bodies (second pass: seek + parse real bodies) ──────────────

    void Parser::expandBodies(ScanResult &result) {
        for (auto &entry : result.fns) {
            if (entry.body_node == kInvalidExpr)
                continue;

            // read token offsets from the UnbodyNode
            auto &unbody = std::get<ast::UnbodyNode>(bld->getExpr(entry.body_node));

            // seek to the start of the body (at '{')
            tok->offset = unbody.token_start;

            // consume '{' then parse the block body
            consume('{');
            auto body_id = parseBlock();

            // replace UnbodyNode with the real parsed body
            bld->getExpr(entry.body_node) = std::move(bld->getExpr(body_id));
        }

        // expand struct bodies (field parsing) in the future
    }

    // ── public API (backward compat) ─────────────────────────────────────

    ProgramResult parseProgram(lexer::TokenStream tokens,
                                ast::AstBuilder &builder,
                                diagnostics::DiagnosticEngine &diags) {
        memory::Arena arena;
        import::SymbolTable syms(arena);
        Parser p{&tokens, &builder, &diags};
        auto result = scan(p, syms);
        p.expandBodies(result);
        return std::move(p.program);
    }

} // namespace zith::parser
