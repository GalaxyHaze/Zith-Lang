#include "lexer/keyword-table.hpp"
#include "lexer/token.hpp"
#include "memory/flat-map.hpp"
#include "memory/optional.hpp"
#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "lexer/lexer.hpp"
#include "memory/source-map.hpp"
#include "parser/operators.hpp"
#include "parser/recovery.hpp"
#include "parser/scan-helpers.hpp"
#include "symbols/symbol-table.hpp"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace zith::parser {

// Parse fn/method parameters with real types (single-pass).
// Advances past closing ')'.
static memory::DynArray<ast::FnParam> parseFnParams(Parser &parser) {
    auto &tok  = *parser.tok;
    auto &diag = *parser.diag;
    auto result = memory::DynArray<ast::FnParam>{parser.bld->arena()};

    while (!tok.is_empty()) {
        if (tok.peek().is_eof() || tok.peek().punc == ')')
            break;

        // destructured params: [a, b, c]: Type
        if (tok.peek().punc == '[') {
            tok.advance();
            memory::DynArray<std::string_view> names{parser.bld->arena()};
            while (!tok.is_empty() && tok.peek().punc != ']') {
                if (tok.peek().is(lexer::TokenKind::Identifier)) {
                    names.push(tok.lexeme());
                    tok.advance();
                } else {
                    tok.advance();
                }
                if (tok.peek().punc == ',')
                    tok.advance();
            }
            if (tok.peek().punc == ']')
                tok.advance();
            auto type_expr = parser.parseOptTypeAnnotation();
            for (auto &n : names)
                result.push({n, type_expr});
            if (tok.peek().punc == ',')
                tok.advance();
            continue;
        }

        if (!tok.peek().is(lexer::TokenKind::Identifier)) {
            diag.report(diagnostics::Severity::Error, diagnostics::err::ExpectedIdent,
                        "expected parameter name", tok.peek().span);
            tok.advance();
            continue;
        }
        auto name = tok.lexeme();
        tok.advance();
        auto type_expr = parser.parseOptTypeAnnotation();
        result.push({name, type_expr});

        if (tok.peek().punc == ',')
            tok.advance();
    }

    if (tok.peek().punc == ')')
        tok.advance();

    return result;
}

static memory::DynArray<ast::GenericParam> tryParseGenericParams(Parser &parser, memory::Arena &arena) {
    auto &tok  = *parser.tok;
    auto &diag = *parser.diag;
    auto params = memory::DynArray<ast::GenericParam>(arena);
    if (!parser.consume('<'))
        return params;
    while (!tok.is_empty() && !tok.peek().is_eof()) {
        if (tok.peek().punc == '>') {
            diag.report(diagnostics::Severity::Error, diagnostics::err::ExpectedIdent,
                        "expected generic parameter name", tok.peek().span);
            break;
        }
        if (!tok.peek().is(lexer::TokenKind::Identifier)) {
            diag.report(diagnostics::Severity::Error, diagnostics::err::ExpectedIdent,
                        "expected generic parameter name", tok.peek().span);
            break;
        }
        auto param_name = tok.lexeme();
        tok.advance();

        if (parser.consume(':'))
            scan_detail::skipTypeExpr(tok);

        params.push(ast::GenericParam{param_name, memory::DynArray<ast::TypeExprId>(arena)});

        if (!parser.consume(','))
            break;
    }
    if (!parser.consume('>')) {
        diag.report(diagnostics::Severity::Error, diagnostics::err::ExpectedExpr,
                    "expected '>' to close generic parameters", tok.peek().span);
        recovery::panic(tok, {lexer::TokenKind::End});
    }
    return params;
}
ScanResult scan(Parser &parser, symbols::SymbolTable &syms) {
    using namespace diagnostics::err;
    auto &tok     = *parser.tok;
    auto &bld     = *parser.bld;
    auto &diag    = *parser.diag;
    auto &program = parser.program;

    ScanResult result{bld.arena()};
    symbols::SymbolVisibility current_vis = symbols::SymbolVisibility::Private;
    int32_t current_mod_depth             = 0;

    memory::Span lastDocSpan{};

    auto resetVis = [&]() {
        current_vis = symbols::SymbolVisibility::Private;
        current_mod_depth = 0;
        lastDocSpan = {};
    };

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
                        char *end = nullptr;
                        long val  = std::strtol(n.data(), &end, 10);
                        if (end == n.data() || val < 1 || val > INT32_MAX) {
                            diag.report(diagnostics::Severity::Error, InvalidImportDepth,
                                        "mod depth must be a positive integer", tok.peek().span);
                            val = 1;
                        }
                        current_mod_depth = static_cast<int32_t>(val);
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
        if (scan_detail::consumeFnModifiers(tok)) {
            tok.advance(); // consume 'fn'

            if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                diag.report(diagnostics::Severity::Error, ExpectedIdent, "expected function name",
                            tok.peek().span);
                tok.advance();
                resetVis();
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            auto generic_params = tryParseGenericParams(parser, bld.arena());

            if (tok.peek().punc != '(') {
                diag.report(diagnostics::Severity::Error, ExpectedExpr,
                            "expected '(' after function name", tok.peek().span);
                resetVis();
                continue;
            }
            tok.advance();

            auto typed_params = parseFnParams(parser);

            // parse return type annotation : TypeExpr
            ast::TypeExprId return_type = ast::kInvalidTypeExpr;
            if (tok.peek().punc == ':') {
                tok.advance();
                return_type = parser.parseTypeExpr();
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
            for (auto &p : typed_params) {
                auto ps =
                    syms.declare(p.name, current_vis, current_mod_depth,
                                 symbols::SymKind::Variable, ast::kInvalidDecl, name_span);
                if (fn_sym != symbols::kInvalidSym)
                    syms.get(fn_sym).members.push(ps);
            }
            lastDocSpan = {};

            auto decl = bld.fnDecl(name, std::move(generic_params),
                                   std::move(typed_params), return_type, body_node,
                                   scan_detail::spanFromOffset(name_span.start, name_span.end));
            program.decls.push(decl);
            if (fn_sym != symbols::kInvalidSym)
                syms.get(fn_sym).decl_id = decl;

            result.fns.push({name, body_span, body_node, 0});
            resetVis();
            continue;
        }

        // ── variable declaration
        if (tok.peek().is(lexer::TokenKind::Variable)) {
            auto kw_span = tok.peek().span;
            auto kw      = tok.lexeme();
            tok.advance();
            if (kw == "global" || kw == "const") {
                scan_detail::scanGlobalOrConst(parser, syms, program.decls,
                                               kw == "const", kw_span,
                                               current_vis, current_mod_depth, lastDocSpan);
            } else {
                diag.report(diagnostics::Severity::Error, TopLevelLetNotAllowed,
                            "You cant declare variables at top-level", kw_span,
                            {"Use `global` (global variable) or `const` (comptime variable) instead"});
                recovery::panic(tok);
                if (tok.peek().punc == ';')
                    tok.advance();
            }
            resetVis();
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
                resetVis();
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            auto generic_params = tryParseGenericParams(parser, bld.arena());

            uint32_t struct_header_start = tok.offset;

            // skip extends/traits tokens to reach '{' or ';'
            while (!tok.is_empty() && tok.peek().punc != '{' && tok.peek().punc != ';') {
                if (tok.peek().is_eof()) break;
                if (tok.peek().punc == '(') { scan_detail::skipBalanced(tok, '(', ')'); continue; }
                if (tok.peek().punc == '<') { scan_detail::skipBalanced(tok, '<', '>'); continue; }
                tok.advance();
            }

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
                decl = bld.structDecl(name, std::move(generic_params),
                                      memory::DynArray<ast::StructField>(bld.arena()),
                                      memory::DynArray<ast::DeclId>(bld.arena()),
                                      ast::kInvalidTypeExpr, name_span);
            } else if (kw == "enum") {
                decl = bld.enumDecl(name, std::move(generic_params),
                                    memory::DynArray<ast::EnumVariant>(bld.arena()), name_span);
            } else if (kw == "union") {
                decl = bld.unionDecl(name, std::move(generic_params),
                                     memory::DynArray<ast::UnionVariant>(bld.arena()), name_span);
            } else {
                decl = bld.componentDecl(name, std::move(generic_params),
                                         memory::DynArray<ast::StructField>(bld.arena()),
                                         name_span);
            }

            // declare struct/union/enum symbol BEFORE body scan
            symbols::SymId struct_sym = symbols::kInvalidSym;
            if (!scan_detail::reportIfDuplicate(syms, diag, name, name_span))
                struct_sym = syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span,
                                          symbols::kInvalidSym, lastDocSpan);

            // ── scan body: register member names, skip implementations ──
            if (tok.peek().punc == '{') {
                uint32_t token_start = tok.offset;
                body_span            = tok.peek().span;

                // shared method handler for all body types
                auto handleMethod = [&](bool must_have_body) {
                    tok.advance(); // consume 'fn'

                    if (tok.peek().punc == '<')
                        scan_detail::skipBalanced(tok, '<', '>');

                    if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                        diag.report(diagnostics::Severity::Error, ExpectedIdent,
                                    "expected method name", tok.peek().span);
                        tok.advance();
                        return;
                    }
                    auto mname_span = tok.peek().span;
                    auto mname      = tok.lexeme();
                    tok.advance();

                    if (tok.peek().punc != '(') {
                        diag.report(diagnostics::Severity::Error, ExpectedExpr,
                                    "expected '(' after method name", tok.peek().span);
                        return;
                    }
                    tok.advance();

                    auto typed_params = parseFnParams(parser);

                    ast::TypeExprId return_type = ast::kInvalidTypeExpr;
                    if (tok.peek().punc == ':') {
                        tok.advance();
                        return_type = parser.parseTypeExpr();
                    }

                    ast::ExprId mn_body_node = ast::kInvalidExpr;
                    uint32_t mn_body_start   = 0;
                    uint32_t mn_body_end     = 0;
                    memory::Span mn_span{};
                    if (tok.peek().punc == '{') {
                        mn_body_start = tok.offset;
                        mn_span       = tok.peek().span;
                        mn_body_end   = scan_detail::skipBody(tok);
                        mn_span       = {mn_span.file, mn_span.start, mn_body_end};
                        mn_body_node  = bld.unbody(mn_span, mn_body_start, mn_body_end);
                    } else if (tok.peek().punc == ';') {
                        if (must_have_body)
                            diag.report(diagnostics::Severity::Error, ExpectedExpr,
                                        "methods in component must have a body", tok.peek().span);
                        tok.advance();
                    } else {
                        diag.report(diagnostics::Severity::Error, ExpectedExpr,
                                    "expected '{' or ';' after method signature", tok.peek().span);
                    }

                    if (!scan_detail::reportIfDuplicate(syms, diag, mname, mname_span)) {
                        auto ms = syms.declare(mname, current_vis, current_mod_depth,
                                               symbols::SymKind::Fn, ast::kInvalidDecl, mname_span);
                        for (auto &p : typed_params) {
                            auto ps = syms.declare(p.name, current_vis, current_mod_depth,
                                                   symbols::SymKind::Variable,
                                                   ast::kInvalidDecl, mname_span);
                            if (ms != symbols::kInvalidSym)
                                syms.get(ms).members.push(ps);
                        }
                        auto method_decl = bld.fnDecl(mname,
                            memory::DynArray<ast::GenericParam>{bld.arena()},
                            std::move(typed_params), return_type, mn_body_node, mname_span);
                        program.decls.push(method_decl);
                        if (ms != symbols::kInvalidSym)
                            syms.get(ms).decl_id = method_decl;
                        if (struct_sym != symbols::kInvalidSym)
                            syms.get(struct_sym).members.push(ms);
                        if (decl != ast::kInvalidDecl) {
                            auto &decl_node = bld.getDecl(decl);
                            if (auto *sn = std::get_if<ast::StructDeclNode>(&decl_node))
                                sn->methods.push(method_decl);
                            else if (auto *cn = std::get_if<ast::ComponentDeclNode>(&decl_node))
                                cn->methods.push(method_decl);
                        }
                        result.fns.push({mname, mn_span, mn_body_node, 0});
                    }
                };

                if (kw == "struct") {
                    memory::FlatMap<std::string, char> field_names;
                    scan_detail::scanBody(tok,
                        [&](lexer::TokenStream &tok) {
                            // skip field qualifiers
                            if (tok.peek().is(lexer::TokenKind::Variable) ||
                                tok.peek().is(lexer::TokenKind::Mutable) ||
                                tok.peek().is(lexer::TokenKind::Ownership)) {
                                while (tok.peek().is(lexer::TokenKind::Variable) ||
                                       tok.peek().is(lexer::TokenKind::Mutable) ||
                                       tok.peek().is(lexer::TokenKind::Ownership))
                                    tok.advance();
                            }

                            // destructure: [a, b, c]: Type
                            if (tok.peek().punc == '[') {
                                tok.advance();
                                while (!tok.is_empty() && tok.peek().punc != ']') {
                                    if (tok.peek().is(lexer::TokenKind::Identifier)) {
                                        auto fname = tok.lexeme();
                                        auto fspan = tok.peek().span;
                                        tok.advance();
                                        if (field_names.contains(std::string(fname)))
                                            diag.report(diagnostics::Severity::Error,
                                                        diagnostics::err::DuplicateDecl,
                                                        "duplicate field '" + std::string(fname) + "'",
                                                        fspan);
                                        else
                                            field_names.insert(std::string(fname), char(1));
                                    } else {
                                        tok.advance();
                                    }
                                    if (tok.peek().punc == ',')
                                        tok.advance();
                                }
                                if (tok.peek().punc == ']')
                                    tok.advance();
                                if (tok.peek().punc == ':')
                                    tok.advance();
                                scan_detail::skipTypeExpr(tok);
                                if (tok.peek().punc == '=') {
                                    tok.advance();
                                    scan_detail::skipExpr(tok);
                                }
                                if (tok.peek().punc == ',')
                                    tok.advance();
                                return;
                            }

                            // field: ident : Type = default ,
                            if (tok.peek().is(lexer::TokenKind::Identifier)) {
                                auto fname = tok.lexeme();
                                auto fspan = tok.peek().span;
                                tok.advance();
                                if (field_names.contains(std::string(fname)))
                                    diag.report(diagnostics::Severity::Error,
                                                diagnostics::err::DuplicateDecl,
                                                "duplicate field '" + std::string(fname) + "'",
                                                fspan);
                                else
                                    field_names.insert(std::string(fname), char(1));
                                if (tok.peek().punc == ':')
                                    tok.advance();
                                scan_detail::skipTypeExpr(tok);
                                if (tok.peek().punc == '=') {
                                    tok.advance();
                                    scan_detail::skipExpr(tok);
                                }
                                if (tok.peek().punc == ',')
                                    tok.advance();
                                return;
                            }

                            if (tok.peek().is(lexer::TokenKind::Annotation)) {
                                tok.advance();
                                return;
                            }

                            tok.advance();
                        },
                        [&](lexer::TokenStream&) { handleMethod(false); },
                        token_start, body_span, bld, body_node);

                } else if (kw == "enum") {
                    scan_detail::scanBody(tok,
                        [&](lexer::TokenStream &tok) {
                            if (tok.peek().is(lexer::TokenKind::Identifier)) {
                                auto v_span = tok.peek().span;
                                auto vname  = tok.lexeme();
                                tok.advance();
                                if (!scan_detail::reportIfDuplicate(syms, diag, vname, v_span)) {
                                    auto vs = syms.declare(vname, symbols::SymbolVisibility::Private, 0,
                                                           symbols::SymKind::Variable,
                                                           ast::kInvalidDecl, v_span);
                                    if (decl != ast::kInvalidDecl && vs != symbols::kInvalidSym)
                                        syms.get(vs).decl_id = decl;
                                }
                                if (tok.peek().punc == '(')
                                    scan_detail::skipBalanced(tok, '(', ')');
                                if (tok.peek().punc == '{')
                                    scan_detail::skipBalanced(tok, '{', '}');
                                if (tok.peek().punc == '=') {
                                    tok.advance();
                                    scan_detail::skipExpr(tok);
                                }
                            } else {
                                tok.advance();
                            }
                            if (tok.peek().punc == ',')
                                tok.advance();
                        },
                        [&](lexer::TokenStream&) { handleMethod(false); },
                        token_start, body_span, bld, body_node);

                } else if (kw == "union") {
                    scan_detail::scanBody(tok,
                        [&](lexer::TokenStream &tok) {
                            if (tok.peek().is(lexer::TokenKind::Type)) {
                                tok.advance();
                            } else {
                                tok.advance();
                            }
                            if (tok.peek().punc == ',')
                                tok.advance();
                        },
                        [&](lexer::TokenStream&) { handleMethod(false); },
                        token_start, body_span, bld, body_node);

                } else if (kw == "component") {
                    memory::FlatMap<std::string, char> field_names;
                    scan_detail::scanBody(tok,
                        [&](lexer::TokenStream &tok) {
                            if (tok.peek().is(lexer::TokenKind::Identifier)) {
                                auto fname = tok.lexeme();
                                auto fspan = tok.peek().span;
                                tok.advance();
                                if (field_names.contains(std::string(fname)))
                                    diag.report(diagnostics::Severity::Error,
                                                diagnostics::err::DuplicateDecl,
                                                "duplicate field '" + std::string(fname) + "'",
                                                fspan);
                                else
                                    field_names.insert(std::string(fname), char(1));
                                if (tok.peek().punc == ':')
                                    tok.advance();
                                scan_detail::skipTypeExpr(tok);
                                if (tok.peek().punc == '=') {
                                    tok.advance();
                                    scan_detail::skipExpr(tok);
                                }
                                if (tok.peek().punc == ',')
                                    tok.advance();
                                return;
                            }

                            if (tok.peek().is(lexer::TokenKind::Annotation)) {
                                tok.advance();
                                return;
                            }

                            tok.advance();
                        },
                        [&](lexer::TokenStream&) { handleMethod(true); },
                        token_start, body_span, bld, body_node);
                }
            }

            program.decls.push(decl);
            lastDocSpan = {};

            ScanEntry entry{name, body_span, body_node, struct_header_start};
            if (kw == "struct")
                result.structs.push(entry);
            else if (kw == "enum")
                result.enums.push(entry);
            else if (kw == "union")
                result.unions.push(entry);
            else if (kw == "component")
                result.components.push(entry);

            resetVis();
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
                resetVis();
                continue;
            }

            auto name_span = tok.peek().span;
            auto name      = tok.lexeme();
            tok.advance();

            auto generic_params = tryParseGenericParams(parser, bld.arena());

            symbols::SymKind sk =
                (kw == "interface") ? symbols::SymKind::Interface : symbols::SymKind::Trait;
            auto decl = (kw == "interface")
                            ? bld.interfaceDecl(name, std::move(generic_params),
                                                memory::DynArray<ast::TraitMethod>(bld.arena()),
                                                name_span)
                            : bld.traitDecl(name, std::move(generic_params),
                                            memory::DynArray<ast::TraitMethod>(bld.arena()),
                                            name_span);

            ast::ExprId body_node = ast::kInvalidExpr;
            memory::Span body_span{};

            // ── scan trait body: register method names ──
            if (tok.peek().punc == '{') {
                uint32_t token_start = tok.offset;
                body_span            = tok.peek().span;

                tok.advance(); // consume '{'
                uint32_t brace_depth = 1;
                while (!tok.is_empty() && brace_depth > 0) {
                    skipComments(tok);
                    if (tok.peek().is_eof())
                        break;
                    if (tok.peek().punc == '}') {
                        if (--brace_depth == 0) {
                            tok.advance();
                            break;
                        }
                        tok.advance();
                        continue;
                    }

                    // trait method: [async | flow | raw | const] fn name(...) : Type
                    if (scan_detail::consumeFnModifiers(tok)) {
                        tok.advance(); // consume 'fn'

                        // skip generic params
                        if (tok.peek().punc == '<')
                            scan_detail::skipBalanced(tok, '<', '>');

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

                        // skip params (...)
                        if (tok.peek().punc == '(')
                            scan_detail::skipBalanced(tok, '(', ')');

                        // skip return type : TypeExpr
                        if (tok.peek().punc == ':') {
                            tok.advance();
                            scan_detail::skipTypeExpr(tok);
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

                body_span = {body_span.file, body_span.start, tok.offset};
                body_node = bld.unbody(body_span, token_start, tok.offset);
            }

            program.decls.push(decl);
            if (!scan_detail::reportIfDuplicate(syms, diag, name, name_span))
                syms.declare(name, current_vis, current_mod_depth, sk, decl, name_span,
                             symbols::kInvalidSym, lastDocSpan);
            lastDocSpan = {};

            result.traits.push({name, body_span, body_node});
            resetVis();
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

            resetVis();
            continue;
        }

        // ── import declaration (from / import / export) ─────────────
        if (tok.peek().is(lexer::TokenKind::Module)) {
            auto kw_span = tok.peek().span;
            auto kw      = tok.lexeme();
            tok.advance();

            // ── type alias: alias Name = TypeExpr ──────────────────
            if (kw == "alias") {
                if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                    diag.report(diagnostics::Severity::Error, ExpectedIdent,
                                "expected alias name", tok.peek().span);
                    resetVis();
                    continue;
                }
                auto name_span = tok.peek().span;
                auto name      = tok.lexeme();
                tok.advance();

                auto generic_params = tryParseGenericParams(parser, bld.arena());

                if (tok.peek().punc != '=') {
                    diag.report(diagnostics::Severity::Error, ExpectedExpr,
                                "expected '=' in type alias", tok.peek().span);
                    resetVis();
                    continue;
                }
                tok.advance(); // consume '='

                auto target_type = parser.parseTypeExpr();

                if (tok.peek().punc == ';')
                    tok.advance();

                auto decl = bld.typeAliasDecl(name, std::move(generic_params), target_type, kw_span);
                program.decls.push(decl);

                auto alias_sym = syms.declare(name, current_vis, current_mod_depth,
                                              symbols::SymKind::Alias, decl, name_span,
                                              symbols::kInvalidSym, lastDocSpan);

                resetVis();
                continue;
            }

            auto parse_path = [&](memory::DynArray<std::string_view> &p) {
                while (!tok.is_empty() && !tok.peek().is_eof()) {
                    if (tok.peek().is(lexer::TokenKind::Identifier)) {
                        auto first = tok.lexeme();
                        const char *seg_start = first.data();
                        const char *seg_end   = first.data() + first.size();
                        tok.advance();
                        // Consume .identifier as part of the same segment (file extension)
                        while (!tok.is_empty() && !tok.peek().is_eof() &&
                               tok.peek().punc == '.') {
                            // '..' is parent path, not an extension
                            if (tok.peek(1).is(lexer::TokenKind::Punctuation) &&
                                tok.peek(1).punc == '.')
                                break;
                            tok.advance(); // consume '.'
                            if (tok.peek().is(lexer::TokenKind::Identifier)) {
                                auto ext = tok.lexeme();
                                seg_end = ext.data() + ext.size();
                                tok.advance();
                            } else {
                                break;
                            }
                        }
                        p.push(std::string_view{seg_start, static_cast<size_t>(seg_end - seg_start)});
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

            auto parse_symbol_list = [&]() -> memory::DynArray<ast::ImportSymbol> {
                auto symbols = memory::DynArray<ast::ImportSymbol>(bld.arena());
                if (tok.peek().punc != '{')
                    return symbols;
                tok.advance(); // consume '{'
                while (!tok.is_empty() && !tok.peek().is_eof() && tok.peek().punc != '}') {
                    if (!tok.peek().is(lexer::TokenKind::Identifier)) {
                        diag.report(diagnostics::Severity::Error, ExpectedIdent,
                                    "expected symbol name in import list", tok.peek().span);
                        break;
                    }
                    auto name = tok.lexeme();
                    tok.advance();
                    std::string_view alias{};
                    if (tok.peek().is(lexer::TokenKind::As)) {
                        tok.advance();
                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            alias = tok.lexeme();
                            tok.advance();
                        } else {
                            diag.report(diagnostics::Severity::Error, ExpectedIdent,
                                        "expected alias name after 'as'", tok.peek().span);
                        }
                    }
                    symbols.push({name, alias});
                    if (tok.peek().punc == ',')
                        tok.advance();
                }
                if (tok.peek().punc == '}')
                    tok.advance();
                else
                    diag.report(diagnostics::Severity::Error, ExpectedExpr,
                                "expected '}' to close import list", tok.peek().span);
                return symbols;
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
                    auto symbols = parse_symbol_list();
                    bool is_asset = !path.empty() && path[0] == "assets";
                    std::string_view alias{};
                    if (tok.peek().is(lexer::TokenKind::As)) {
                        tok.advance();
                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            alias = tok.lexeme();
                            tok.advance();
                        }
                    }
                    if (is_asset && alias.empty())
                        diag.report(diagnostics::Severity::Error, ImportError,
                                    "assets import requires an alias using 'as'", kw_span);
                    auto decl = bld.importDecl(std::move(path), std::move(symbols), alias,
                                               /*is_from=*/kw == "from", /*is_export=*/kw == "export",
                                               is_asset, import_depth, kw_span);
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
                    auto symbols = parse_symbol_list();
                    std::string_view alias{};
                    if (tok.peek().is(lexer::TokenKind::As)) {
                        tok.advance();
                        if (tok.peek().is(lexer::TokenKind::Identifier)) {
                            alias = tok.lexeme();
                            tok.advance();
                        }
                    }
                    auto decl =
                        bld.importDecl(std::move(path), std::move(symbols), alias,
                                       false, false, false, import_depth, kw_span);
                    program.decls.push(decl);
                }
            }
            resetVis();
            continue;
        }

        // ── @annotation — skip ─────────────────────────────────────
        if (tok.peek().is(lexer::TokenKind::Annotation)) {
            tok.advance();
            continue;
        }

        // ── unknown token → skip to ; or next declaration ─────────
        diag.report(diagnostics::Severity::Error, ExpectedExpr, "unexpected token",
                    tok.peek().span);
        recovery::panic(tok, {lexer::TokenKind::Fn, lexer::TokenKind::Struct,
                              lexer::TokenKind::Trait, lexer::TokenKind::Interface,
                              lexer::TokenKind::Implement, lexer::TokenKind::Module,
                              lexer::TokenKind::Variable});
        if (tok.peek().punc == ';')
            tok.advance();
        resetVis();
    }

    return result;
}

} // namespace zith::parser
