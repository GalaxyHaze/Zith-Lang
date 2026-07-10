#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "lexer/lexer.hpp"
#include "memory/flat-map.hpp"
#include "parser/scan-helpers.hpp"

#include <cstdlib>
#include <string>
#include <cstring>

namespace zith::parser {

using namespace diagnostics::err;

static std::string_view arenaStr(memory::Arena &arena, const std::string &s) {
    auto *buf = static_cast<char *>(arena.alloc(s.size()));
    std::memcpy(buf, s.data(), s.size());
    return {buf, s.size()};
}

// ── expand bodies (second pass: seek + parse real bodies) ──────────────

void Parser::expandBodies(ScanResult &result) {
    for (auto &entry : result.fns) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        // Params + return type already parsed during scan (single-pass).
        // Only need to expand the body (UnbodyNode → BlockNode).

        // Position at body start
        auto &unbody = std::get<ast::UnbodyNode>(bld->getExpr(entry.body_node));
        tok->offset  = unbody.token_start;

        consume('{');
        auto body_id = parseBlock();

        bld->getExpr(entry.body_node) = std::move(bld->getExpr(body_id));
    }

    for (auto &entry : result.structs) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        tok->offset = entry.struct_header_start;

        // 1. extends parent
        ast::TypeExprId extends_parent = ast::kInvalidTypeExpr;
        if (tok->peek().is(lexer::TokenKind::Trait) && tok->lexeme() == "extends") {
            tok->advance();
            extends_parent = parseTypeExpr();
        }

        // 2. traits
        memory::DynArray<ast::TypeExprId> traits{bld->arena()};
        if (tok->peek().punc == ':') {
            tok->advance();
            traits = parseDelimited(*tok, bld->arena(), '{', [this]() { return parseTypeExpr(); });
        }

        // 3. body fields
        consume('{');
        memory::DynArray<ast::StructField> fields{bld->arena()};
        while (!tok->is_empty() && !tok->peek().is_eof() && tok->peek().punc != '}') {
            // visibility
            if (tok->peek().is(lexer::TokenKind::Visibility)) {
                tok->advance();
                if (tok->peek().punc == '(' && tok->lexeme() == "mod")
                    scan_detail::skipBalanced(*tok, '(', ')');
                if (tok->peek().punc == ')')
                    tok->advance();
                continue;
            }

            // method fn — skip to ';' or body
            if (scan_detail::consumeFnModifiers(*tok)) {
                tok->advance(); // consume 'fn'
                if (tok->peek().is(lexer::TokenKind::Identifier))
                    tok->advance();
                while (!tok->is_empty() && !tok->peek().is_eof()) {
                    if (tok->peek().punc == ';') { tok->advance(); break; }
                    if (tok->peek().punc == '{') { scan_detail::skipBalanced(*tok, '{', '}'); break; }
                    if (tok->peek().punc == '}') break;
                    tok->advance();
                }
                continue;
            }

            // field qualifiers (bind + mut)
            ast::FieldBind bind = ast::FieldBind::Auto;
            while (tok->peek().is(lexer::TokenKind::Variable) ||
                   tok->peek().is(lexer::TokenKind::Mutable)) {
                auto lex = tok->lexeme();
                if (lex == "auto")     bind = ast::FieldBind::Auto;
                if (lex == "const")    bind = ast::FieldBind::Const;
                if (lex == "let")      bind = ast::FieldBind::Let;
                if (lex == "var")      bind = ast::FieldBind::Var;
                if (lex == "global")   bind = ast::FieldBind::Global;
                if (lex == "comptime") bind = ast::FieldBind::Comptime;
                tok->advance();
            }

            // destructure: [a, b, c]: Type
            if (tok->peek().punc == '[') {
                tok->advance();
                memory::DynArray<std::string_view> names{bld->arena()};
                while (!tok->is_empty() && tok->peek().punc != ']') {
                    if (tok->peek().is(lexer::TokenKind::Identifier)) {
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
                auto type_expr = parseOptTypeAnnotation();
                ast::ExprId default_val = ast::kInvalidExpr;
                if (tok->peek().punc == '=') {
                    tok->advance();
                    default_val = parseExpr();
                }
                for (auto &n : names)
                    fields.push({n, type_expr, default_val, bind, ast::FieldKind::Struct});
                if (tok->peek().punc == ',')
                    tok->advance();
                continue;
            }

            // field: ident : Type = default ,
            if (tok->peek().is(lexer::TokenKind::Identifier)) {
                auto fname = tok->lexeme();
                tok->advance();
                auto type_expr = parseOptTypeAnnotation();
                ast::ExprId default_val = ast::kInvalidExpr;
                if (tok->peek().punc == '=') {
                    tok->advance();
                    default_val = parseExpr();
                }
                fields.push({fname, type_expr, default_val, bind, ast::FieldKind::Struct});
                if (tok->peek().punc == ',')
                    tok->advance();
                continue;
            }

            // unrecognized token — skip to next ',' or '}' for recovery
            while (!tok->is_empty() && !tok->peek().is_eof() &&
                   tok->peek().punc != ',' && tok->peek().punc != '}')
                tok->advance();
            if (tok->peek().punc == ',')
                tok->advance();
        }
        if (tok->peek().punc == '}')
            tok->advance();

        // 4. write back to StructDeclNode
        for (auto &decl_id : program.decls) {
            auto &decl = bld->getDecl(decl_id);
            if (auto *sn = std::get_if<ast::StructDeclNode>(&decl)) {
                if (sn->name == entry.name && sn->fields.empty()) {
                    sn->fields = std::move(fields);
                    sn->extends_parent = extends_parent;
                    sn->traits = std::move(traits);
                    break;
                }
            }
        }
    }

    for (auto &entry : result.enums) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        tok->offset = entry.struct_header_start;
        consume('{');

        memory::DynArray<ast::EnumVariant> variants{bld->arena()};
        while (!tok->is_empty() && !tok->peek().is_eof() && tok->peek().punc != '}') {
            // method fn — skip to ';' or body
            if (scan_detail::consumeFnModifiers(*tok)) {
                tok->advance(); // consume 'fn'
                if (tok->peek().is(lexer::TokenKind::Identifier))
                    tok->advance();
                while (!tok->is_empty() && !tok->peek().is_eof()) {
                    if (tok->peek().punc == ';') { tok->advance(); break; }
                    if (tok->peek().punc == '{') { scan_detail::skipBalanced(*tok, '{', '}'); break; }
                    if (tok->peek().punc == '}') break;
                    tok->advance();
                }
                continue;
            }

            // variant: Identifier [(...) | {...}] [= expr]
            if (tok->peek().is(lexer::TokenKind::Identifier)) {
                auto vname = tok->lexeme();
                tok->advance();

                ast::ExprId discriminant = ast::kInvalidExpr;
                memory::DynArray<ast::StructField> fields{bld->arena()};

                // tuple variant: Variant(Type, Type)
                if (tok->peek().punc == '(') {
                    tok->advance();
                    uint32_t idx = 0;
                    while (!tok->is_empty() && !tok->peek().is_eof() && tok->peek().punc != ')') {
                        auto field_type = parseTypeExpr();
                        auto field_name = arenaStr(bld->arena(), "_" + std::to_string(idx++));
                        fields.push({field_name, field_type, ast::kInvalidExpr,
                                     ast::FieldBind::Auto, ast::FieldKind::Enum});
                        if (tok->peek().punc == ',')
                            tok->advance();
                    }
                    if (tok->peek().punc == ')')
                        tok->advance();
                }

                // struct variant: Variant { name: Type, name: Type }
                if (tok->peek().punc == '{') {
                    tok->advance();
                    while (!tok->is_empty() && !tok->peek().is_eof() && tok->peek().punc != '}') {
                        if (tok->peek().is(lexer::TokenKind::Identifier)) {
                            auto fname = tok->lexeme();
                            tok->advance();
                            auto field_type = parseOptTypeAnnotation();
                            fields.push({fname, field_type, ast::kInvalidExpr,
                                         ast::FieldBind::Auto, ast::FieldKind::Enum});
                        }
                        if (tok->peek().punc == ',')
                            tok->advance();
                    }
                    if (tok->peek().punc == '}')
                        tok->advance();
                }

                // discriminant: = expr
                if (tok->peek().punc == '=') {
                    tok->advance();
                    discriminant = parseExpr();
                }

                variants.push({vname, std::move(fields), discriminant});

                if (tok->peek().punc == ',')
                    tok->advance();
                continue;
            }

            // skip annotations
            if (tok->peek().is(lexer::TokenKind::Annotation)) {
                tok->advance();
                continue;
            }

            // unrecognized token — skip to next ',' or '}'
            while (!tok->is_empty() && !tok->peek().is_eof() &&
                   tok->peek().punc != ',' && tok->peek().punc != '}')
                tok->advance();
            if (tok->peek().punc == ',')
                tok->advance();
        }
        if (tok->peek().punc == '}')
            tok->advance();

        // write back to EnumDeclNode
        for (auto &decl_id : program.decls) {
            auto &decl = bld->getDecl(decl_id);
            if (auto *en = std::get_if<ast::EnumDeclNode>(&decl)) {
                if (en->name == entry.name && en->variants.empty()) {
                    en->variants = std::move(variants);
                    break;
                }
            }
        }
    }

    for (auto &entry : result.components) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        tok->offset = entry.struct_header_start;
        consume('{');

        memory::DynArray<ast::StructField> fields{bld->arena()};
        while (!tok->is_empty() && !tok->peek().is_eof() && tok->peek().punc != '}') {
            // method fn — skip to ';' or body
            if (scan_detail::consumeFnModifiers(*tok)) {
                tok->advance(); // consume 'fn'
                if (tok->peek().is(lexer::TokenKind::Identifier))
                    tok->advance();
                while (!tok->is_empty() && !tok->peek().is_eof()) {
                    if (tok->peek().punc == ';') { tok->advance(); break; }
                    if (tok->peek().punc == '{') { scan_detail::skipBalanced(*tok, '{', '}'); break; }
                    if (tok->peek().punc == '}') break;
                    tok->advance();
                }
                continue;
            }

            // field: ident : Type = default ,
            if (tok->peek().is(lexer::TokenKind::Identifier)) {
                auto fname = tok->lexeme();
                tok->advance();
                auto type_expr = parseOptTypeAnnotation();
                ast::ExprId default_val = ast::kInvalidExpr;
                if (tok->peek().punc == '=') {
                    tok->advance();
                    default_val = parseExpr();
                }
                fields.push({fname, type_expr, default_val, ast::FieldBind::Auto, ast::FieldKind::Component});
                if (tok->peek().punc == ',')
                    tok->advance();
                continue;
            }

            // skip annotations
            if (tok->peek().is(lexer::TokenKind::Annotation)) {
                tok->advance();
                continue;
            }

            // unrecognized token — skip to next ',' or '}'
            while (!tok->is_empty() && !tok->peek().is_eof() &&
                   tok->peek().punc != ',' && tok->peek().punc != '}')
                tok->advance();
            if (tok->peek().punc == ',')
                tok->advance();
        }
        if (tok->peek().punc == '}')
            tok->advance();

        // write back to ComponentDeclNode
        for (auto &decl_id : program.decls) {
            auto &decl = bld->getDecl(decl_id);
            if (auto *cn = std::get_if<ast::ComponentDeclNode>(&decl)) {
                if (cn->name == entry.name && cn->fields.empty()) {
                    cn->fields = std::move(fields);
                    break;
                }
            }
        }
    }

    for (auto &entry : result.unions) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        tok->offset = entry.struct_header_start;
        consume('{');

        memory::DynArray<ast::UnionVariant> variants{bld->arena()};
        memory::FlatMap<std::string, char> variant_names;
        while (!tok->is_empty() && !tok->peek().is_eof() && tok->peek().punc != '}') {
            // method fn — skip to ';' or body
            if (scan_detail::consumeFnModifiers(*tok)) {
                tok->advance(); // consume 'fn'
                if (tok->peek().is(lexer::TokenKind::Identifier))
                    tok->advance();
                while (!tok->is_empty() && !tok->peek().is_eof()) {
                    if (tok->peek().punc == ';') { tok->advance(); break; }
                    if (tok->peek().punc == '{') { scan_detail::skipBalanced(*tok, '{', '}'); break; }
                    if (tok->peek().punc == '}') break;
                    tok->advance();
                }
                continue;
            }

            // unnamed variant: Type
            if (tok->peek().is(lexer::TokenKind::Type)) {
                auto vname = tok->lexeme();
                auto vspan = tok->peek().span;
                tok->advance();
                if (variant_names.contains(std::string(vname)))
                    diag->report(diagnostics::Severity::Error, diagnostics::err::DuplicateDecl,
                                "duplicate union variant '" + std::string(vname) + "'", vspan);
                else
                    variant_names.insert(std::string(vname), char(1));
                variants.push({vname, ast::kInvalidTypeExpr});
                if (tok->peek().punc == ',')
                    tok->advance();
                continue;
            }

            // skip annotations
            if (tok->peek().is(lexer::TokenKind::Annotation)) {
                tok->advance();
                continue;
            }

            // unrecognized token — skip to next ',' or '}'
            while (!tok->is_empty() && !tok->peek().is_eof() &&
                   tok->peek().punc != ',' && tok->peek().punc != '}')
                tok->advance();
            if (tok->peek().punc == ',')
                tok->advance();
        }
        if (tok->peek().punc == '}')
            tok->advance();

        // write back to UnionDeclNode
        for (auto &decl_id : program.decls) {
            auto &decl = bld->getDecl(decl_id);
            if (auto *un = std::get_if<ast::UnionDeclNode>(&decl)) {
                if (un->name == entry.name && un->variants.empty()) {
                    un->variants = std::move(variants);
                    break;
                }
            }
        }
    }

    // trait body expansion deferred — structure is already scanned
}

} // namespace zith::parser
