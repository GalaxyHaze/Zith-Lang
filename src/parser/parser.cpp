#include "parser.hpp"
#include "memory/source-map.hpp"
#include "lexer/lexer.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-table.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
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
        auto &s = tok->peek().span;
        return {tok->file_base + s.start, s.end - s.start};
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
        if (tok->peek().punc == c) {
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

        if (peek().punc == '(') {
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
            if (peek().punc == '}')
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
            auto val = eof() || peek().punc == '}'
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
        while (!eof() && peek().punc != ')') {
            if (!peek().is(TokenKind::Identifier)) {
                diag->report(Severity::Error, ExpectedIdent,
                               "expected parameter name", peek().span);
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
            if (tok.is_empty()) return tok.offset;
            uint32_t depth = 1;
            tok.advance();
            while (!tok.is_empty() && depth > 0) {
                if (tok.peek().kind == TokenKind::End) break;
                if (tok.peek().punc == '{') depth++;
                else if (tok.peek().punc == '}') depth--;
                tok.advance();
            }
            return tok.offset;
        }

        static const char *symKindName(import::SymKind k) {
            switch (k) {
                case import::SymKind::Fn:        return "a fn";
                case import::SymKind::Struct:    return "a struct";
                case import::SymKind::Trait:     return "a trait";
                case import::SymKind::Enum:      return "an enum";
                case import::SymKind::Alias:     return "an alias";
                case import::SymKind::Variable:  return "a variable";
                case import::SymKind::Module:    return "a module";
                case import::SymKind::Component: return "a component";
            }
            return "unknown";
        }

        [[nodiscard]] bool reportIfDuplicate(import::SymbolTable &syms,
                                              diagnostics::DiagnosticEngine &diag,
                                              std::string_view name,
                                              memory::Span span,
                                              import::SymId skip_id = import::kInvalidSym) {
            auto existing = syms.lookup(name);
            if (existing == import::kInvalidSym || existing == skip_id)
                return false;
            auto &prev = syms.get(existing);
            std::string msg = "duplicate symbol '";
            msg += name;
            msg += "' — previous declaration is ";
            msg += symKindName(prev.kind);
            diag.report(Severity::Error, DuplicateDecl, std::move(msg), span);
            return true;
        }

    } // anonymous namespace

    ScanResult scan(Parser &parser, import::SymbolTable &syms, bool sema) {
        (void)sema;
        auto &tok = *parser.tok;
        auto &bld = *parser.bld;
        auto &diag = *parser.diag;
        auto &program = parser.program;

        ScanResult result{memory::SessionArena};
        import::SymbolVisibility current_vis = import::SymbolVisibility::Private;
        int32_t current_mod_depth = 0;

        while (!tok.is_empty()) {
            if (tok.peek().is_eof()) break;

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
                            current_mod_depth = 1;
                            if (!n.empty() && n[0] >= '0' && n[0] <= '9')
                                current_mod_depth = n[0] - '0';
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
                    diag.report(Severity::Error, ExpectedIdent,
                                "expected function name", tok.peek().span);
                    tok.advance();
                    current_vis = import::SymbolVisibility::Private;
                    current_mod_depth = 0;
                    continue;
                }

                auto name_span = tok.peek().span;
                auto name = tok.lexeme();
                tok.advance();

                if (tok.peek().punc != '(') {
                    diag.report(Severity::Error, ExpectedExpr,
                                "expected '(' after function name", tok.peek().span);
                    current_vis = import::SymbolVisibility::Private;
                    current_mod_depth = 0;
                    continue;
                }
                tok.advance();

                memory::DynArray<std::string_view> params{memory::SessionArena};
                memory::DynArray<memory::Span> param_spans{memory::SessionArena};
                while (!tok.is_empty()) {
                    if (tok.peek().is_eof()) break;
                    if (tok.peek().punc == ')')
                        break;

                    if (!tok.peek().is(TokenKind::Identifier)) {
                        diag.report(Severity::Error, ExpectedIdent,
                                    "expected parameter name", tok.peek().span);
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
                uint32_t token_start = 0;
                uint32_t token_end = 0;
                memory::Span body_span{};

                if (tok.peek().punc == '{') {
                    token_start = tok.offset;
                    body_span = tok.peek().span;
                    token_end = skip_body_tokens(tok);
                    body_span = {body_span.file, body_span.start, token_end};
                    body_node = bld.unbody(body_span, token_start, token_end);
                }

                auto fn_sym = import::kInvalidSym;
                if (!reportIfDuplicate(syms, diag, name, name_span)) {
                    fn_sym = syms.declare(name, current_vis, current_mod_depth,
                                           import::SymKind::Fn, ast::kInvalidDecl, name_span);
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
                current_vis = import::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            // ── struct declaration ─────────────────────────────────────
            if (tok.peek().is(TokenKind::Struct)) {
                tok.advance();

                if (!tok.peek().is(TokenKind::Identifier)) {
                    diag.report(Severity::Error, ExpectedIdent,
                                "expected struct name", tok.peek().span);
                    tok.advance();
                    current_vis = import::SymbolVisibility::Private;
                    current_mod_depth = 0;
                    continue;
                }

                auto name_span = tok.peek().span;
                auto name = tok.lexeme();
                tok.advance();

                auto decl = bld.structDecl(name, {});
                program.decls.push(decl);
                if (!reportIfDuplicate(syms, diag, name, name_span))
                    syms.declare(name, current_vis, current_mod_depth,
                                 import::SymKind::Struct, decl, name_span);

                result.structs.push({name, {}, kInvalidExpr});
                current_vis = import::SymbolVisibility::Private;
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
                            long val = std::strtol(n.data(), &end, 10);
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
                    memory::DynArray<std::string_view> path{memory::SessionArena};
                    parse_path(path);
                    if (path.empty()) {
                        diag.report(Severity::Error, ImportError,
                                    "expected import path after '" + std::string(kw) + "'",
                                    tok.peek().span);
                    } else {
                        auto import_depth = parse_depth();
                        auto decl = bld.importDecl(std::move(path), {},
                                                    kw == "from" || kw == "export", kw == "export",
                                                    import_depth);
                        program.decls.push(decl);
                    }
                } else if (kw == "import") {
                    memory::DynArray<std::string_view> path{memory::SessionArena};
                    parse_path(path);
                    if (path.empty()) {
                        diag.report(Severity::Error, ImportError,
                                    "expected import path after 'import'",
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
                current_vis = import::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            // ── unknown token → skip ───────────────────────────────────
            diag.report(Severity::Error, ExpectedExpr,
                        "unexpected token", tok.peek().span);
            tok.advance();
            current_vis = import::SymbolVisibility::Private;
            current_mod_depth = 0;
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
