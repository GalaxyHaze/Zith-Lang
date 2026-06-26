#include "parser.hpp"

#include "diagnostics/error-codes.hpp"
#include "lexer/lexer.hpp"
#include "parser/scan-helpers.hpp"

#include <cstdlib>
#include <string>

namespace zith::parser {

using namespace diagnostics::err;

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
                    scan_detail::skipBalanced(*tok, '{', '}');
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
                scan_detail::skipTypeExpr(*tok);
            }
            if (tok->peek().punc == '=') {
                tok->advance();
                scan_detail::skipExpr(*tok);
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
                scan_detail::skipTypeExpr(*tok);
                type_lexeme = tok->lexeme();
            }
            out_fields.push({fname, type_lexeme});
            if (tok->peek().punc == '=') {
                tok->advance();
                scan_detail::skipExpr(*tok);
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

// ── expand bodies (second pass: seek + parse real bodies) ──────────────

void Parser::expandBodies(ScanResult &result) {
    for (auto &entry : result.fns) {
        if (entry.body_node == ast::kInvalidExpr)
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
                ast::TypeExprId type_expr = parseOptTypeAnnotation();
                for (auto &n : names)
                    typed_params.push({n, type_expr});
            } else if (tok->peek().is(lexer::TokenKind::Identifier)) {
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
                    fn->params      = std::move(typed_params);
                    fn->return_type = return_type;
                    break;
                }
            }
        }

        // Now parse the body — position should be at `{`
        auto &unbody = std::get<ast::UnbodyNode>(bld->getExpr(entry.body_node));
        tok->offset  = unbody.token_start;

        consume('{');
        auto body_id = parseBlock();

        bld->getExpr(entry.body_node) = std::move(bld->getExpr(body_id));
    }

    memory::Arena field_arena;
    memory::DynArray<FieldInfo> fields_tmp{field_arena};

    for (auto &entry : result.structs) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        auto &unbody = std::get<ast::UnbodyNode>(bld->getExpr(entry.body_node));

        tok->offset = unbody.token_start;

        fields_tmp.clear();
        parseStructBody(*this, entry.span, fields_tmp);
    }

    // enum/union/trait body expansion deferred — structure is already scanned
    // result.enums, result.unions, result.traits entries exist with body spans
}

} // namespace zith::parser
