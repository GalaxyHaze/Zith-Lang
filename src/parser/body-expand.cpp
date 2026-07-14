#include "parser.hpp"

#include "ast/ast-node-utils.hpp"
#include "diagnostics/error-codes.hpp"
#include "memory/flat-map.hpp"
#include "parser/scan-helpers.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace zith::parser {

using namespace diagnostics::err;

namespace {

static std::string_view arenaStr(memory::Arena &arena, const std::string &s) {
    auto *buf = static_cast<char *>(arena.alloc(s.size()));
    std::memcpy(buf, s.data(), s.size());
    return {buf, s.size()};
}

void skipToNextBodyItem(lexer::TokenStream &tok) {
    while (!tok.is_empty() && !tok.peek().is_eof() && tok.peek().punc != ',' &&
           tok.peek().punc != '}')
        tok.advance();
    if (tok.peek().punc == ',')
        tok.advance();
}

template <typename ConsumeItemFn>
void consumeBodyItems(Parser &parser, ConsumeItemFn &&consume_item) {
    auto &tok = *parser.tok;
    while (!tok.is_empty() && !tok.peek().is_eof() && tok.peek().punc != '}') {
        if (scan_detail::consumeFnModifiers(tok)) {
            scan_detail::skipMethodMember(tok);
            continue;
        }
        if (consume_item())
            continue;
        skipToNextBodyItem(tok);
    }
    if (tok.peek().punc == '}')
        tok.advance();
}

template <typename ApplyFn>
void updateNamedDecl(ast::AstBuilder &builder, ast::ProgramNode &program, std::string_view name,
                     ApplyFn &&apply) {
    for (auto &decl_id : program.decls) {
        auto &decl = builder.getDecl(decl_id);
        if (apply(decl, name))
            break;
    }
}

template <typename PreItemFn>
auto parseStructLikeFields(Parser &parser, ast::FieldKind kind,
                           const scan_detail::FieldItemOptions &options, PreItemFn &&pre_item)
    -> memory::DynArray<ast::StructField> {
    auto &tok = *parser.tok;
    auto &bld = *parser.bld;

    memory::DynArray<ast::StructField> fields{bld.arena()};
    while (!tok.is_empty() && !tok.peek().is_eof() && tok.peek().punc != '}') {
        if (pre_item())
            continue;

        if (scan_detail::consumeFnModifiers(tok)) {
            scan_detail::skipMethodMember(tok);
            continue;
        }

        if (scan_detail::consumeFieldItem(
                tok, options, [&]() { return parser.parseOptTypeAnnotation(); },
                [&]() { return parser.parseExpr(); }, []() { return ast::kInvalidExpr; },
                [&](std::string_view fname, memory::Span, ast::TypeExprId type_expr,
                    ast::ExprId default_val, ast::FieldBind bind) {
                    auto effective_bind =
                        (kind == ast::FieldKind::Component) ? ast::FieldBind::Auto : bind;
                    fields.push({fname, type_expr, default_val, effective_bind, kind});
                })) {
            continue;
        }

        skipToNextBodyItem(tok);
    }

    if (tok.peek().punc == '}')
        tok.advance();
    return fields;
}

auto parseEnumVariantFields(Parser &parser) -> memory::DynArray<ast::StructField> {
    auto &tok = *parser.tok;
    auto &bld = *parser.bld;

    memory::DynArray<ast::StructField> fields{bld.arena()};

    if (tok.peek().punc == '(') {
        tok.advance();
        uint32_t idx = 0;
        while (!tok.is_empty() && !tok.peek().is_eof() && tok.peek().punc != ')') {
            auto field_type = parser.parseTypeExpr();
            auto field_name = arenaStr(bld.arena(), "_" + std::to_string(idx++));
            fields.push({field_name, field_type, ast::kInvalidExpr, ast::FieldBind::Auto,
                         ast::FieldKind::Enum});
            if (tok.peek().punc == ',')
                tok.advance();
        }
        if (tok.peek().punc == ')')
            tok.advance();
    }

    if (tok.peek().punc == '{') {
        tok.advance();
        while (!tok.is_empty() && !tok.peek().is_eof() && tok.peek().punc != '}') {
            if (scan_detail::consumeFieldItem(
                    tok, scan_detail::FieldItemOptions{},
                    [&]() { return parser.parseOptTypeAnnotation(); },
                    []() { return ast::kInvalidExpr; }, []() { return ast::kInvalidExpr; },
                    [&](std::string_view fname, memory::Span, ast::TypeExprId type_expr,
                        ast::ExprId, ast::FieldBind) {
                        fields.push({fname, type_expr, ast::kInvalidExpr, ast::FieldBind::Auto,
                                     ast::FieldKind::Enum});
                    })) {
                continue;
            }

            skipToNextBodyItem(tok);
        }
        if (tok.peek().punc == '}')
            tok.advance();
    }

    return fields;
}

bool consumeEnumVariant(Parser &parser, memory::DynArray<ast::EnumVariant> &variants) {
    auto &tok = *parser.tok;
    if (tok.peek().is(lexer::TokenKind::Annotation)) {
        tok.advance();
        return true;
    }
    if (!tok.peek().is(lexer::TokenKind::Identifier))
        return false;

    auto vname = tok.lexeme();
    tok.advance();

    auto fields       = parseEnumVariantFields(parser);
    auto discriminant = ast::kInvalidExpr;
    if (tok.peek().punc == '=') {
        tok.advance();
        discriminant = parser.parseExpr();
    }

    variants.push({vname, std::move(fields), discriminant});
    if (tok.peek().punc == ',')
        tok.advance();
    return true;
}

bool consumeUnionVariant(Parser &parser, diagnostics::DiagnosticEngine &diag,
                         memory::FlatMap<std::string, char> &variant_names,
                         memory::DynArray<ast::UnionVariant> &variants) {
    auto &tok = *parser.tok;
    if (tok.peek().is(lexer::TokenKind::Annotation)) {
        tok.advance();
        return true;
    }
    if (!tok.peek().is(lexer::TokenKind::Type))
        return false;

    auto vname = tok.lexeme();
    auto vspan = tok.peek().span;
    tok.advance();
    if (variant_names.contains(std::string(vname))) {
        diag.report(diagnostics::Severity::Error, diagnostics::err::DuplicateDecl,
                    "duplicate union variant '" + std::string(vname) + "'", vspan);
    } else {
        variant_names.insert(std::string(vname), char(1));
    }
    variants.push({vname, ast::kInvalidTypeExpr});
    if (tok.peek().punc == ',')
        tok.advance();
    return true;
}

} // namespace
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
        auto fields =
            parseStructLikeFields(*this, ast::FieldKind::Struct,
                                  scan_detail::FieldItemOptions{
                                      .support_destructure = true,
                                      .skip_qualifiers     = true,
                                      .parse_bind          = true,
                                  },
                                  [&]() {
                                      if (!tok->peek().is(lexer::TokenKind::Visibility))
                                          return false;
                                      tok->advance();
                                      if (tok->peek().punc == '(' && tok->lexeme() == "mod")
                                          scan_detail::skipBalanced(*tok, '(', ')');
                                      if (tok->peek().punc == ')')
                                          tok->advance();
                                      return true;
                                  });

        updateNamedDecl(*bld, program, entry.name, [&](ast::DeclNode &decl, std::string_view name) {
            auto *sn = ast::asStruct(decl);
            if (!sn || sn->name != name || !sn->fields.empty())
                return false;
            sn->fields         = std::move(fields);
            sn->extends_parent = extends_parent;
            sn->traits         = std::move(traits);
            return true;
        });
    }

    for (auto &entry : result.enums) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        tok->offset = entry.struct_header_start;
        consume('{');

        memory::DynArray<ast::EnumVariant> variants{bld->arena()};
        consumeBodyItems(*this, [&]() { return consumeEnumVariant(*this, variants); });

        updateNamedDecl(*bld, program, entry.name, [&](ast::DeclNode &decl, std::string_view name) {
            auto *en = ast::asEnum(decl);
            if (!en || en->name != name || !en->variants.empty())
                return false;
            en->variants = std::move(variants);
            return true;
        });
    }

    for (auto &entry : result.components) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        tok->offset = entry.struct_header_start;
        consume('{');

        auto fields =
            parseStructLikeFields(*this, ast::FieldKind::Component, scan_detail::FieldItemOptions{},
                                  []() { return false; });

        updateNamedDecl(*bld, program, entry.name, [&](ast::DeclNode &decl, std::string_view name) {
            auto *cn = ast::asComponent(decl);
            if (!cn || cn->name != name || !cn->fields.empty())
                return false;
            cn->fields = std::move(fields);
            return true;
        });
    }

    for (auto &entry : result.unions) {
        if (entry.body_node == ast::kInvalidExpr)
            continue;

        tok->offset = entry.struct_header_start;
        consume('{');

        memory::DynArray<ast::UnionVariant> variants{bld->arena()};
        memory::FlatMap<std::string, char> variant_names;
        consumeBodyItems(
            *this, [&]() { return consumeUnionVariant(*this, *diag, variant_names, variants); });

        updateNamedDecl(*bld, program, entry.name, [&](ast::DeclNode &decl, std::string_view name) {
            auto *un = ast::asUnion(decl);
            if (!un || un->name != name || !un->variants.empty())
                return false;
            un->variants = std::move(variants);
            return true;
        });
    }

    // trait body expansion deferred — structure is already scanned
}

} // namespace zith::parser
