#include "ast/ast-printer.hpp"

namespace zith::ast {

namespace {

template <class... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void print_indent(FILE *out, int depth) {
    for (int i = 0; i < depth; ++i)
        std::fprintf(out, "  ");
}

void print_stmt(StmtId id, const AstBuilder &bld, FILE *out, int depth);
void print_expr(ExprId id, const AstBuilder &bld, FILE *out, int depth);

void print_expr_node(const ExprNode &node, const AstBuilder &bld, FILE *out, int depth) {
    std::visit(overloaded{
                   [&](const LitValue &n) {
                       std::fprintf(out, "LitValue(%s, \"%.*s\")\n",
                                    n.kind == LitKind::Int      ? "Int"
                                    : n.kind == LitKind::Float  ? "Float"
                                    : n.kind == LitKind::Bool   ? "Bool"
                                    : n.kind == LitKind::String ? "String"
                                    : n.kind == LitKind::Char   ? "Char"
                                                                : "Nil",
                                    (int)n.raw.size(), n.raw.data());
                   },
                   [&](const IdentNode &n) {
                       std::fprintf(out, "Ident(%.*s)\n", (int)n.name.size(), n.name.data());
                   },
                   [&](const BinaryNode &n) {
                       const char *op = nullptr;
                       switch (n.op) {
                       case BinaryOp::Add:
                           op = "+";
                           break;
                       case BinaryOp::Sub:
                           op = "-";
                           break;
                       case BinaryOp::Mul:
                           op = "*";
                           break;
                       case BinaryOp::Div:
                           op = "/";
                           break;
                       case BinaryOp::Rest:
                           op = "%";
                           break;
                       case BinaryOp::Eq:
                           op = "==";
                           break;
                       case BinaryOp::Ne:
                           op = "!=";
                           break;
                       case BinaryOp::Lt:
                           op = "<";
                           break;
                       case BinaryOp::Le:
                           op = "<=";
                           break;
                       case BinaryOp::Gt:
                           op = ">";
                           break;
                       case BinaryOp::Ge:
                           op = ">=";
                           break;
                       case BinaryOp::And:
                           op = "and";
                           break;
                       case BinaryOp::Or:
                           op = "or";
                           break;
                       case BinaryOp::Xor:
                           op = "^";
                           break;
                       case BinaryOp::Shl:
                           op = "<<";
                           break;
                       case BinaryOp::Shr:
                           op = ">>";
                           break;
                       }
                       std::fprintf(out, "Binary(%s)\n", op);
                       print_indent(out, depth + 1);
                       print_expr(n.lhs, bld, out, depth + 1);
                       print_indent(out, depth + 1);
                       print_expr(n.rhs, bld, out, depth + 1);
                   },
                   [&](const UnaryNode &n) {
                       const char *op = nullptr;
                       switch (n.op) {
                       case UnaryOp::Neg:
                           op = "-";
                           break;
                       case UnaryOp::Not:
                           op = "!";
                           break;
                       case UnaryOp::Ref:
                           op = "&";
                           break;
                       case UnaryOp::Deref:
                           op = "*";
                           break;
                       }
                       std::fprintf(out, "Unary(%s)\n", op);
                       print_indent(out, depth + 1);
                       print_expr(n.operand, bld, out, depth + 1);
                   },
                   [&](const CallNode &n) {
                       std::fprintf(out, "Call\n");
                       print_indent(out, depth + 1);
                       std::fprintf(out, "callee: ");
                       print_expr(n.callee, bld, out, depth + 1);
                       for (auto arg : n.args) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "arg: ");
                           print_expr(arg, bld, out, depth + 1);
                       }
                   },
                   [&](const BlockNode &n) {
                       std::fprintf(out, "Block\n");
                       for (auto s : n.stmts) {
                           print_indent(out, depth + 1);
                           print_stmt(s, bld, out, depth + 1);
                       }
                       if (n.trailing != kInvalidExpr) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "trailing: ");
                           print_expr(n.trailing, bld, out, depth + 1);
                       }
                   },
                   [&](const IfNode &n) {
                       std::fprintf(out, "If\n");
                       print_indent(out, depth + 1);
                       std::fprintf(out, "cond: ");
                       print_expr(n.cond, bld, out, depth + 1);
                       print_indent(out, depth + 1);
                       std::fprintf(out, "then: ");
                       print_expr(n.then_branch, bld, out, depth + 1);
                       if (n.else_branch != kInvalidExpr) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "else: ");
                           print_expr(n.else_branch, bld, out, depth + 1);
                       }
                   },
                   [&](const WhileNode &n) {
                       std::fprintf(out, "While\n");
                       print_indent(out, depth + 1);
                       std::fprintf(out, "cond: ");
                       print_expr(n.cond, bld, out, depth + 1);
                       print_indent(out, depth + 1);
                       std::fprintf(out, "body: ");
                       print_expr(n.body, bld, out, depth + 1);
                   },
                   [&](const FieldNode &n) {
                       std::fprintf(out, "Field(%.*s)\n", (int)n.field.size(), n.field.data());
                       print_indent(out, depth + 1);
                       print_expr(n.object, bld, out, depth + 1);
                   },
                   [&](const IndexNode &n) {
                       std::fprintf(out, "Index\n");
                       print_indent(out, depth + 1);
                       print_expr(n.object, bld, out, depth + 1);
                       print_indent(out, depth + 1);
                       print_expr(n.index, bld, out, depth + 1);
                   },
                   [&](const RangeNode &n) {
                       std::fprintf(out, "Range\n");
                       print_indent(out, depth + 1);
                       print_expr(n.lhs, bld, out, depth + 1);
                       print_indent(out, depth + 1);
                       print_expr(n.rhs, bld, out, depth + 1);
                   },
                   [&](const UnbodyNode &n) {
                       std::fprintf(out, "Unbody(spans [%u..%u], tokens [%u..%u])\n",
                                    n.body_span.start, n.body_span.end, n.token_start, n.token_end);
                   },
               },
               node);
}

void print_expr(ExprId id, const AstBuilder &bld, FILE *out, int depth) {
    if (id == kInvalidExpr) {
        std::fprintf(out, "<invalid>\n");
        return;
    }
    auto &node = bld.getExpr(id);
    print_expr_node(node, bld, out, depth);
}

void print_stmt(StmtId id, const AstBuilder &bld, FILE *out, int depth) {
    if (id == kInvalidStmt) {
        std::fprintf(out, "<invalid stmt>\n");
        return;
    }
    auto &node = bld.getStmt(id);
    std::visit(overloaded{
                    [&](const LetNode &n) {
                        auto var_name = n.names.empty() ? std::string_view{} : n.names[0];
                        std::fprintf(out, "Let(%.*s, mut=%s)\n", (int)var_name.size(), var_name.data(),
                                     n.mut ? "true" : "false");
                       if (n.init != kInvalidExpr) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "init: ");
                           print_expr(n.init, bld, out, depth + 1);
                       }
                   },
                   [&](const AssignNode &n) {
                       std::fprintf(out, "Assign\n");
                       print_indent(out, depth + 1);
                       print_expr(n.target, bld, out, depth + 1);
                       print_indent(out, depth + 1);
                       print_expr(n.value, bld, out, depth + 1);
                   },
                   [&](const RetNode &n) {
                       std::fprintf(out, "Return\n");
                       if (n.value != kInvalidExpr) {
                           print_indent(out, depth + 1);
                           print_expr(n.value, bld, out, depth + 1);
                       }
                   },
                   [&](ExprId expr_id) { print_expr(expr_id, bld, out, depth); },
               },
               node);
}

void print_decl(DeclId id, const AstBuilder &bld, FILE *out, int depth) {
    if (id == kInvalidDecl) {
        std::fprintf(out, "<invalid decl>\n");
        return;
    }
    auto &node = bld.getDecl(id);
    std::visit(overloaded{
                   [&](const FnDeclNode &n) {
                       std::fprintf(out, "Fn(%.*s)\n", (int)n.name.size(), n.name.data());
                        if (n.params.size() > 0) {
                            print_indent(out, depth + 1);
                            std::fprintf(out, "params: ");
                            for (size_t i = 0; i < n.params.size(); ++i) {
                                if (i > 0)
                                    std::fprintf(out, ", ");
                                std::fprintf(out, "%.*s", (int)n.params[i].name.size(),
                                             n.params[i].name.data());
                            }
                            std::fprintf(out, "\n");
                        }
                       if (n.body != kInvalidExpr) {
                           print_indent(out, depth + 1);
                           print_expr(n.body, bld, out, depth + 1);
                       }
                   },
                   [&](const StructDeclNode &n) {
                       std::fprintf(out, "Struct(%.*s)\n", (int)n.name.size(), n.name.data());
                       for (auto &f : n.fields) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "field %.*s\n", (int)f.name.size(), f.name.data());
                       }
                   },
                   [&](const EnumDeclNode &n) {
                       std::fprintf(out, "Enum(%.*s)\n", (int)n.name.size(), n.name.data());
                       for (auto &v : n.variants) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "variant %.*s\n", (int)v.name.size(), v.name.data());
                       }
                   },
                   [&](const UnionDeclNode &n) {
                       std::fprintf(out, "Union(%.*s)\n", (int)n.name.size(), n.name.data());
                       for (auto &v : n.variants) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "variant %.*s\n", (int)v.name.size(), v.name.data());
                       }
                   },
                   [&](const ComponentDeclNode &n) {
                       std::fprintf(out, "Component(%.*s)\n", (int)n.name.size(), n.name.data());
                   },
                   [&](const TraitDeclNode &n) {
                       std::fprintf(out, "Trait(%.*s)\n", (int)n.name.size(), n.name.data());
                       for (auto &m : n.methods) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "method %.*s\n", (int)m.name.size(), m.name.data());
                       }
                   },
                   [&](const InterfaceDeclNode &n) {
                       std::fprintf(out, "Interface(%.*s)\n", (int)n.name.size(), n.name.data());
                       for (auto &m : n.methods) {
                           print_indent(out, depth + 1);
                           std::fprintf(out, "method %.*s\n", (int)m.name.size(), m.name.data());
                       }
                   },
                   [&](const ImportNode &n) {
                       std::fprintf(out, "%s%s(", n.is_export ? "Export " : "",
                                    n.is_from ? "From" : "Import");
                       for (size_t i = 0; i < n.path.size(); ++i) {
                           if (i > 0)
                               std::fprintf(out, "::");
                           std::fprintf(out, "%.*s", (int)n.path[i].size(), n.path[i].data());
                       }
                       if (n.import_depth == -1)
                           std::fprintf(out, "(..)");
                       else if (n.import_depth > 1)
                           std::fprintf(out, "(%d)", n.import_depth);
                       if (!n.alias.empty())
                           std::fprintf(out, " as %.*s", (int)n.alias.size(), n.alias.data());
                       std::fprintf(out, ")\n");
                   },
               },
               node);
}

} // anonymous namespace

void printAST(const ProgramNode &program, const AstBuilder &builder, FILE *out) {
    std::fprintf(out, "Program\n");
    for (auto decl : program.decls) {
        std::fprintf(out, "  ");
        print_decl(decl, builder, out, 1);
    }
    std::fflush(out);
}

} // namespace zith::ast
