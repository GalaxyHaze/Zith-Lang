#include "resolver.hpp"
#include "diagnostics/error-codes.hpp"

namespace zith::symbols {

static std::string joinWith(const memory::DynArray<std::string_view> &path) {
    std::string result;
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0)
            result += '.';
        result.append(path[i].data(), path[i].size());
    }
    return result;
}

Resolver::Resolver(SymbolTable &syms, ImportManager &import_mgr, ast::AstBuilder &builder,
                   diagnostics::DiagnosticEngine &diags)
    : syms_(syms), import_mgr_(import_mgr), builder_(builder), diags_(diags),
      resolved_(builder.arena()) {}

void Resolver::resolveProgram(ast::ProgramNode &program) {
    // First pass: process import declarations (validate & register aliases)
    for (auto decl_id : program.decls) {
        auto &decl = builder_.getDecl(decl_id);
        if (auto *import = std::get_if<ast::ImportNode>(&decl)) {
            auto import_key = joinWith(import->path);
            if (!import_mgr_.isLoaded(import_key))
                continue;
            if (!import->alias.empty()) {
                auto mod_sym = syms_.lookup(import_key);
                if (mod_sym != kInvalidSym)
                    syms_.declare(import->alias, SymbolVisibility::Public, 0, SymKind::Alias,
                                  ast::kInvalidDecl, {}, mod_sym);
            }
        }
    }

    // Second pass: resolve identifiers in all declaration bodies
    for (auto decl_id : program.decls) {
        auto &decl = builder_.getDecl(decl_id);
        if (!std::get_if<ast::ImportNode>(&decl))
            resolveDecl(decl_id);
    }
}

void Resolver::resolveDecl(ast::DeclId id) {
    auto &decl = builder_.getDecl(id);
    std::visit(
        [&](auto &n) {
            using T = std::decay_t<decltype(n)>;
            if constexpr (std::is_same_v<T, ast::FnDeclNode>) {
                auto fn_scope = syms_.enterScope();
                for (auto &p : n.params)
                    syms_.declareInScope(fn_scope, p.name);
                if (n.body != ast::kInvalidExpr)
                    resolveExpr(n.body);
                syms_.exitScope();
            }
        },
        decl);
}

void Resolver::resolveStmt(ast::StmtId id) {
    auto &node = builder_.getStmt(id);
    struct StmtVisitor {
        Resolver &r;
        void operator()(const ast::LetNode &n) {
            if (n.init != ast::kInvalidExpr)
                r.resolveExpr(n.init);
        }
        void operator()(const ast::AssignNode &n) {
            r.resolveExpr(n.target);
            r.resolveExpr(n.value);
        }
        void operator()(const ast::RetNode &n) {
            if (n.value != ast::kInvalidExpr)
                r.resolveExpr(n.value);
        }
        void operator()(ast::ExprId expr_id) {
            r.resolveExpr(expr_id);
        }
    };
    std::visit(StmtVisitor{*this}, node);
}

void Resolver::resolveExpr(ast::ExprId id) {
    if (id == ast::kInvalidExpr)
        return;

    auto &node = builder_.getExpr(id);

    struct ExprVisitor {
        Resolver &r;
        ast::ExprId expr_id;

        void operator()(const ast::IdentNode &n) {
            auto sym = r.lookupQualified(n.name);
            if (sym == kInvalidSym) {
                auto dot = n.name.find('.');
                // Qualified name failures already reported specific diagnostics
                if (dot == std::string_view::npos) {
                    r.diags_.report(diagnostics::Severity::Error, diagnostics::err::UndefinedIdent,
                                    "undefined identifier '" + std::string(n.name) + "'", {});
                }
            }
            r.setResolved(expr_id, sym);
        }
        void operator()(const ast::BinaryNode &n) {
            r.resolveExpr(n.lhs);
            r.resolveExpr(n.rhs);
        }
        void operator()(const ast::UnaryNode &n) {
            r.resolveExpr(n.operand);
        }
        void operator()(const ast::CallNode &n) {
            r.resolveExpr(n.callee);
            for (auto arg : n.args)
                r.resolveExpr(arg);
        }
        void operator()(const ast::BlockNode &n) {
            r.syms_.enterScope();
            for (auto s : n.stmts)
                r.resolveStmt(s);
            if (n.trailing != ast::kInvalidExpr)
                r.resolveExpr(n.trailing);
            r.syms_.exitScope();
        }
        void operator()(const ast::IfNode &n) {
            r.resolveExpr(n.cond);
            r.resolveExpr(n.then_branch);
            if (n.else_branch != ast::kInvalidExpr)
                r.resolveExpr(n.else_branch);
        }
        void operator()(const ast::WhileNode &n) {
            r.resolveExpr(n.cond);
            r.resolveExpr(n.body);
        }
        void operator()(const ast::FieldNode &n) {
            r.resolveExpr(n.object);
        }
        void operator()(const ast::IndexNode &n) {
            r.resolveExpr(n.object);
            r.resolveExpr(n.index);
        }
        void operator()(const ast::RangeNode &n) {
            r.resolveExpr(n.lhs);
            r.resolveExpr(n.rhs);
        }
        void operator()(const ast::LitValue &) {}
        void operator()(const ast::UnbodyNode &) {}
        void operator()(const ast::IntrinsicNode &n) {
            for (auto arg : n.args)
                r.resolveExpr(arg);
        }
        void operator()(const ast::MacroCallNode &n) {
            for (auto arg : n.args)
                r.resolveExpr(arg);
        }
    };

    std::visit(ExprVisitor{*this, id}, node);
}

SymId Resolver::followAliases(SymId id) const {
    while (id != kInvalidSym) {
        auto &data = syms_.get(id);
        if (data.kind != SymKind::Alias || data.target == kInvalidSym)
            break;
        id = data.target;
    }
    return id;
}

SymId Resolver::lookupQualified(std::string_view name) {
    auto dot = name.find('.');
    if (dot == std::string_view::npos)
        return lookupUnqualified(name);

    auto first = name.substr(0, dot);
    auto rest  = name.substr(dot + 1);

    auto sym = lookupUnqualified(first);
    if (sym == kInvalidSym) {
        diags_.report(diagnostics::Severity::Error, diagnostics::err::UndefinedIdent,
                      "undefined identifier '" + std::string(first) + "'", {});
        return kInvalidSym;
    }

    sym = followAliases(sym);
    if (sym == kInvalidSym)
        return kInvalidSym;

    auto &data = syms_.get(sym);
    if (!isNamespaceLike(data.kind)) {
        diags_.report(diagnostics::Severity::Error, diagnostics::err::NotNamespace,
                      "'" + std::string(first) + "' is not a namespace, so '" + std::string(name) +
                          "' is invalid",
                      {});
        return kInvalidSym;
    }

    auto scope     = data.scope;
    auto remaining = rest;
    std::string prefix(first);
    for (;;) {
        auto next_dot = remaining.find('.');
        auto segment =
            next_dot == std::string_view::npos ? remaining : remaining.substr(0, next_dot);
        auto found = syms_.lookupInScope(segment, scope);
        if (found == kInvalidSym) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::NoMember,
                          "no member '" + std::string(segment) + "' in '" + prefix + "'", {});
            return kInvalidSym;
        }
        found = followAliases(found);
        if (found == kInvalidSym)
            return kInvalidSym;
        if (next_dot == std::string_view::npos)
            return found;
        auto &next_data = syms_.get(found);
        if (!isNamespaceLike(next_data.kind)) {
            diags_.report(diagnostics::Severity::Error, diagnostics::err::NotNamespace,
                          "'" + prefix + "." + std::string(segment) +
                              "' is not a namespace, so the remainder of '" + std::string(name) +
                              "' cannot be resolved",
                          {});
            return kInvalidSym;
        }
        scope = next_data.scope;
        prefix += "." + std::string(segment);
        remaining = remaining.substr(next_dot + 1);
    }
}

SymId Resolver::lookupUnqualified(std::string_view name) {
    auto sym = syms_.lookup(name);
    return sym != kInvalidSym ? followAliases(sym) : kInvalidSym;
}

bool Resolver::isNamespaceLike(SymKind kind) const {
    return kind == SymKind::Module || kind == SymKind::Struct || kind == SymKind::Enum ||
           kind == SymKind::Trait || kind == SymKind::Interface || kind == SymKind::Union ||
           kind == SymKind::Component;
}

void Resolver::setResolved(ast::ExprId id, SymId sym) {
    if (id == ast::kInvalidExpr)
        return;
    if (id >= resolved_.size()) {
        if (id >= resolved_.capacity())
            resolved_.reserve(id + 1);
        while (resolved_.size() <= id)
            resolved_.push(kInvalidSym);
    }
    resolved_[id] = sym;
}

SymId Resolver::resolvedSym(ast::ExprId id) const {
    if (id >= resolved_.size())
        return kInvalidSym;
    return resolved_[id];
}

bool Resolver::hasResolvedSym(ast::ExprId id) const {
    return id < resolved_.size() && resolved_[id] != kInvalidSym;
}

memory::DynArray<SymId> Resolver::takeResolvedTable() {
    return std::move(resolved_);
}

} // namespace zith::symbols
