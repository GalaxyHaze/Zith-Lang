#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "symbols/symbol-table.hpp"
#include "lexer/lexer.hpp"
#include "memory/source-map.hpp"
#include "parser/operators.hpp"
#include "parser/scan-helpers.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace zith::parser {

ScanResult scan(Parser &parser, symbols::SymbolTable &syms) {
    using namespace diagnostics::err;
    auto &tok     = *parser.tok;
    auto &bld     = *parser.bld;
    auto &diag    = *parser.diag;
    auto &program = parser.program;

    ScanResult result{bld.arena()};
    symbols::SymbolVisibility current_vis = symbols::SymbolVisibility::Private;
    int32_t current_mod_depth            = 0;

    memory::Span lastDocSpan{};

    while (!tok.is_empty()) {
        if (tok.peek().is_eof())
            break;

        // ── capture doc comments ───────────────────────────────────
        if (tok.peek().is(lexer::TokenKind::Docs)) {
            if (lastDocSpan.start == 0 && lastDocSpan.end == 0)
                lastDocSpan = tok.peek().span;
            else
                lastDocSpan.end = tok.peek().span.end;
            tok.advance();
            continue;
        }

        // ── skip plain comments ────────────────────────────────────
        if (tok.peek().is(lexer::TokenKind::Comments)) {
            tok.advance();
            continue;
        }

        // ── visibility modifier (pub / mod) ────────────────────────
        if (tok.peek().is(lexer::TokenKind::Visibility)) {
            auto kw = tok.lexeme();
            tok.advance();

            if (kw == "pub") {
                current_vis = symbols::SymbolVisibility::Public;
                continue;
            }

            if (kw == "mod") {
                current_vis = symbols::SymbolVisibility::Module;
                if (tok.peek().punc == '(') {
                    tok.advance();
                    if (tok.peek().punc == '.') {
                        tok.advance(); // '.'
                        tok.advance(); // '.'
                        current_mod_depth = -1;
                        if (tok.peek().punc == ')')
                            tok.advance();
                    } else if (tok.peek().is(lexer::TokenKind::LitVal)) {
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
        if (tok.peek().is(lexer::TokenKind::Fn)) {
            tok.advance();

            if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                diag.report(diagnostics::Severity::Error, ExpectedIdent, "expected function name",
                            tok.peek().span);
                tok.advance();
                current_vis       = symbols::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            if (tok.peek().punc != '(') {
                diag.report(diagnostics::Severity::Error, ExpectedExpr,
                            "expected '(' after function name", tok.peek().span);
                current_vis       = symbols::SymbolVisibility::Private;
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
                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
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
                        scan_detail::skipTypeExpr(tok);
                    }
                    if (tok.peek().punc == ',')
                        tok.advance();
                    continue;
                }

                if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                    diag.report(diagnostics::Severity::Error, ExpectedIdent,
                                "expected parameter name", tok.peek().span);
                    tok.advance();
                    continue;
                }
                param_spans.push(tok.peek().span);
                params.push(tok.lexeme());
                tok.advance();

                // skip type annotation : TypeExpr
                if (tok.peek().punc == ':') {
                    tok.advance();
                    scan_detail::skipTypeExpr(tok);
                }

                if (tok.peek().punc == ',')
                    tok.advance();
            }
            if (tok.peek().punc == ')')
                tok.advance();

            // skip return type annotation : TypeExpr
            if (tok.peek().punc == ':') {
                tok.advance();
                scan_detail::skipTypeExpr(tok);
            }

            ast::ExprId body_node = ast::kInvalidExpr;
            uint32_t token_start  = 0;
            uint32_t token_end    = 0;
            memory::Span body_span{};

            if (tok.peek().punc == '{') {
                token_start = tok.offset;
                body_span   = tok.peek().span;
                token_end   = scan_detail::skipBody(tok);
                body_span   = {body_span.file, body_span.start, token_end};
                body_node   = bld.unbody(body_span, token_start, token_end);
            }

            auto fn_sym =
                syms.declare(name, current_vis, current_mod_depth, symbols::SymKind::Fn,
                             ast::kInvalidDecl, name_span, symbols::kInvalidSym, lastDocSpan);
            for (size_t i = 0; i < params.size(); i++) {
                auto ps =
                    syms.declare(params[i], current_vis, current_mod_depth,
                                 symbols::SymKind::Variable, ast::kInvalidDecl, param_spans[i]);
                syms.get(fn_sym).members.push(ps);
            }
            lastDocSpan = {};

            auto decl = bld.fnDecl(name, std::move(params), body_node,
                                   scan_detail::spanFromOffset(name_span.start, name_span.end));
            program.decls.push(decl);
            if (fn_sym != symbols::kInvalidSym)
                syms.get(fn_sym).decl_id = decl;

            result.fns.push({name, body_span, body_node, param_token_start});
            current_vis       = symbols::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── struct-like declaration (struct / enum / union / component) ─
        if (tok.peek().is(lexer::TokenKind::Struct)) {
            auto kw = tok.lexeme();
            tok.advance();

            if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                diag.report(diagnostics::Severity::Error, ExpectedIdent,
                            "expected name after '" + std::string(kw) + "'", tok.peek().span);
                tok.advance();
                current_vis       = symbols::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            symbols::SymKind sk;
            if (kw == "enum")
                sk = symbols::SymKind::Enum;
            else if (kw == "union")
                sk = symbols::SymKind::Union;
            else if (kw == "component")
                sk = symbols::SymKind::Component;
            else
                sk = symbols::SymKind::Struct;

            auto decl             = ast::kInvalidDecl;
            ast::ExprId body_node = ast::kInvalidExpr;
            memory::Span body_span{};

            // create appropriate AST node
            if (kw == "struct") {
                decl = bld.structDecl(name, memory::DynArray<ast::StructField>(bld.arena()),
                                      name_span);
            } else if (kw == "enum") {
                decl =
                    bld.enumDecl(name, memory::DynArray<ast::EnumVariant>(bld.arena()), name_span);
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
                        if (tok.peek().is(lexer::TokenKind::Visibility)) {
                            auto vkw = tok.lexeme();
                            tok.advance();
                            if (vkw == "mod" && tok.peek().punc == '(')
                                scan_detail::skipBalanced(tok, '(', ')');
                            continue;
                        }

                        // method: fn / async fn / raw fn / const fn / flowing fn
                        if (tok.peek().is(lexer::TokenKind::Fn) ||
                            tok.peek().is(lexer::TokenKind::Control) /* flowing */) {
                            if (tok.peek().is(lexer::TokenKind::Control))
                                tok.advance(); // 'flowing'
                            if (tok.peek().is(lexer::TokenKind::Fn))
                                tok.advance();
                            if (tok.peek().is(lexer::TokenKind::Identifier)) {
                                auto mname_span = tok.peek().span;
                                auto mname      = tok.lexeme();
                                tok.advance();
                                // register method name as fn member
                                if (!scan_detail::reportIfDuplicate(syms, diag, mname,
                                                                    mname_span)) {
                                    auto ms = syms.declare(mname, current_vis, current_mod_depth,
                                                           symbols::SymKind::Fn, ast::kInvalidDecl,
                                                           mname_span);
                                    if (decl != ast::kInvalidDecl && ms != symbols::kInvalidSym)
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
                                    scan_detail::skipBalanced(tok, '{', '}');
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
                                if (tok.peek().is(lexer::TokenKind::Identifier)) {
                                    auto fname_span = tok.peek().span;
                                    auto fname      = tok.lexeme();
                                    tok.advance();
                                    if (!scan_detail::reportIfDuplicate(syms, diag, fname,
                                                                        fname_span)) {
                                        auto fs =
                                            syms.declare(fname, symbols::SymbolVisibility::Private,
                                                         0, symbols::SymKind::Variable,
                                                         ast::kInvalidDecl, fname_span);
                                        if (decl != ast::kInvalidDecl && fs != symbols::kInvalidSym)
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
                                scan_detail::skipTypeExpr(tok);
                            }
                            // skip default value
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_detail::skipExpr(tok);
                            }
                            if (tok.peek().punc == ',')
                                tok.advance();
                            continue;
                        }

                        // field: ident : Type = default ,
                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            auto fname_span = tok.peek().span;
                            auto fname      = tok.lexeme();
                            tok.advance();
                            if (!scan_detail::reportIfDuplicate(syms, diag, fname, fname_span)) {
                                auto fs = syms.declare(fname, symbols::SymbolVisibility::Private, 0,
                                                       symbols::SymKind::Variable, ast::kInvalidDecl,
                                                       fname_span);
                                if (decl != ast::kInvalidDecl && fs != symbols::kInvalidSym)
                                    syms.get(fs).decl_id = decl;
                            }
                            // skip type annotation
                            if (tok.peek().punc == ':') {
                                tok.advance();
                                scan_detail::skipTypeExpr(tok);
                            }
                            // skip default value
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_detail::skipExpr(tok);
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

                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            auto v_span = tok.peek().span;
                            auto vname  = tok.lexeme();
                            tok.advance();

                            if (!scan_detail::reportIfDuplicate(syms, diag, vname, v_span)) {
                                auto vs = syms.declare(vname, symbols::SymbolVisibility::Private, 0,
                                                       symbols::SymKind::Variable, ast::kInvalidDecl,
                                                       v_span);
                                if (decl != ast::kInvalidDecl && vs != symbols::kInvalidSym)
                                    syms.get(vs).decl_id = decl;
                            }

                            // tuple variant: Variant(Type1, Type2)
                            if (tok.peek().punc == '(')
                                scan_detail::skipBalanced(tok, '(', ')');
                            // struct variant: Variant { field: Type }
                            if (tok.peek().punc == '{')
                                scan_detail::skipBalanced(tok, '{', '}');
                            // discriminant assignment: Variant = 1
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_detail::skipExpr(tok);
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

                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            auto v_span = tok.peek().span;
                            auto vname  = tok.lexeme();
                            tok.advance();

                            if (!scan_detail::reportIfDuplicate(syms, diag, vname, v_span)) {
                                auto vs = syms.declare(vname, symbols::SymbolVisibility::Private, 0,
                                                       symbols::SymKind::Variable, ast::kInvalidDecl,
                                                       v_span);
                                if (decl != ast::kInvalidDecl && vs != symbols::kInvalidSym)
                                    syms.get(vs).decl_id = decl;
                            }

                            // skip type annotation: variant: Type
                            if (tok.peek().punc == ':') {
                                tok.advance();
                                scan_detail::skipTypeExpr(tok);
                            }
                            // skip default
                            if (tok.peek().punc == '=') {
                                tok.advance();
                                scan_detail::skipExpr(tok);
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
                    scan_detail::skipBalanced(tok, '{', '}');
                }

                body_span = {body_span.file, body_span.start, tok.offset};
                body_node = bld.unbody(body_span, token_start, tok.offset);
            }

            program.decls.push(decl);
            if (!scan_detail::reportIfDuplicate(syms, diag, name, name_span))
                syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span,
                             symbols::kInvalidSym, lastDocSpan);
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

            current_vis       = symbols::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── trait / interface declaration ────────────────────────────
        if (tok.peek().is(lexer::TokenKind::Trait) || tok.peek().is(lexer::TokenKind::Interface)) {
            auto kw = tok.lexeme();
            tok.advance();

            if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                diag.report(diagnostics::Severity::Error, ExpectedIdent,
                            "expected name after '" + std::string(kw) + "'", tok.peek().span);
                tok.advance();
                current_vis       = symbols::SymbolVisibility::Private;
                current_mod_depth = 0;
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            symbols::SymKind sk =
                (kw == "interface") ? symbols::SymKind::Interface : symbols::SymKind::Trait;
            auto decl = (kw == "interface")
                            ? bld.interfaceDecl(
                                  name, memory::DynArray<ast::TraitMethod>(bld.arena()), name_span)
                            : bld.traitDecl(name, memory::DynArray<ast::TraitMethod>(bld.arena()),
                                            name_span);

            ast::ExprId body_node = ast::kInvalidExpr;
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
                    if (tok.peek().is(lexer::TokenKind::Fn)) {
                        tok.advance();
                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            auto m_span = tok.peek().span;
                            auto mname  = tok.lexeme();
                            tok.advance();
                            if (!scan_detail::reportIfDuplicate(syms, diag, mname, m_span)) {
                                auto ms =
                                    syms.declare(mname, symbols::SymbolVisibility::Private, 0,
                                                 symbols::SymKind::Fn, ast::kInvalidDecl, m_span);
                                if (decl != ast::kInvalidDecl && ms != symbols::kInvalidSym)
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
                                scan_detail::skipBalanced(tok, '{', '}');
                                break;
                            }
                            if (tok.peek().punc == '}')
                                break;
                            tok.advance();
                        }
                        continue;
                    }

                    // associated type: type Foo;
                    if (tok.peek().is(lexer::TokenKind::Typedef)) {
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
            if (!scan_detail::reportIfDuplicate(syms, diag, name, name_span))
                syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span,
                             symbols::kInvalidSym, lastDocSpan);
            lastDocSpan = {};

            result.traits.push({name, body_span, body_node});
            current_vis       = symbols::SymbolVisibility::Private;
            current_mod_depth = 0;
            continue;
        }

        // ── implement declaration ────────────────────────────────────
        if (tok.peek().is(lexer::TokenKind::Implement)) {
            tok.advance();

            // syntax: implement Type for Trait { ... }
            // or:      implement Trait for Type { ... }
            // during scan: skip to '{' and create unbody
            while (!tok.is_empty() && !tok.peek().is_eof()) {
                if (tok.peek().punc == '{') {
                    scan_detail::skipBalanced(tok, '{', '}');
                    break;
                }
                if (tok.peek().punc == ';') {
                    tok.advance();
                    break;
                }
                tok.advance();
            }

            current_vis       = symbols::SymbolVisibility::Private;
            current_mod_depth = 0;
            lastDocSpan       = {};
            continue;
        }

        // ── import declaration (from / import / export) ─────────────
        if (tok.peek().is(lexer::TokenKind::Module)) {
            auto kw_span = tok.peek().span;
            auto kw      = tok.lexeme();
            tok.advance();

            auto parse_path = [&](memory::DynArray<std::string_view> &p) {
                while (!tok.is_empty() && !tok.peek().is_eof()) {
                    if (tok.peek().is(lexer::TokenKind::Identifier)) {
                        p.push(tok.lexeme());
                        tok.advance();
                    } else if (tok.peek().punc == '/') {
                        tok.advance();
                    } else if (tok.peek().is(lexer::TokenKind::Punctuation) &&
                               tok.peek().punc == '.') {
                        if (tok.peek(1).is(lexer::TokenKind::Punctuation) &&
                            tok.peek(1).punc == '.') {
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
                    } else if (tok.peek().is(lexer::TokenKind::LitVal)) {
                        auto n = tok.lexeme();
                        tok.advance();
                        char *end = nullptr;
                        long val  = std::strtol(n.data(), &end, 10);
                        if (end == n.data() || val <= 0 || val > INT32_MAX) {
                            diag.report(diagnostics::Severity::Error, InvalidImportDepth,
                                        "import depth must be a positive integer or '..'",
                                        tok.peek().span);
                            val = 1;
                        }
                        if (tok.peek().punc == ')')
                            tok.advance();
                        return static_cast<int32_t>(val);
                    } else {
                        diag.report(diagnostics::Severity::Error, InvalidImportDepth,
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
                    diag.report(diagnostics::Severity::Error, ImportError,
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
                    diag.report(diagnostics::Severity::Error, ImportError,
                                "expected import path after 'import'", tok.peek().span);
                } else {
                    auto import_depth = parse_depth();
                    std::string_view alias{};
                    if (tok.peek().is(lexer::TokenKind::As)) {
                        tok.advance();
                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            alias = tok.lexeme();
                            tok.advance();
                        }
                    }
                    auto decl =
                        bld.importDecl(std::move(path), alias, false, false, import_depth, kw_span);
                    program.decls.push(decl);
                }
            }
            current_vis       = symbols::SymbolVisibility::Private;
            current_mod_depth = 0;
            lastDocSpan       = {};
            continue;
        }

        // ── unknown token → skip ───────────────────────────────────
        diag.report(diagnostics::Severity::Error, ExpectedExpr, "unexpected token",
                    tok.peek().span);
        tok.advance();
        lastDocSpan       = {};
        current_vis       = symbols::SymbolVisibility::Private;
        current_mod_depth = 0;
    }

    return result;
}

} // namespace zith::parser
