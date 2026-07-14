#include "resolver.hpp"
#include "ast/ast-node-utils.hpp"
#include "common/format.hpp"
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
        if (auto *import = ast::asImport(decl)) {
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
        if (!ast::asImport(decl))
            resolveDecl(decl_id);
    }
}

void Resolver::resolveDecl(ast::DeclId id) {
    auto &decl = builder_.getDecl(id);
    switch (ast::declKind(decl)) {
    case ast::DeclKind::Fn: {
        const auto &n = std::get<ast::FnDeclNode>(decl);
        auto fn_scope = syms_.enterScope();
        for (auto &p : n.params)
            syms_.declareInScope(fn_scope, p.name);
        if (n.body != ast::kInvalidExpr)
            resolveExpr(n.body);
        syms_.exitScope();
        break;
    }
    case ast::DeclKind::Struct:
    case ast::DeclKind::Enum:
    case ast::DeclKind::Union:
    case ast::DeclKind::Component:
    case ast::DeclKind::Trait:
    case ast::DeclKind::Interface:
    case ast::DeclKind::Import:
    case ast::DeclKind::TypeAlias:
    case ast::DeclKind::Global:
        break;
    }
}

void Resolver::resolveStmt(ast::StmtId id) {
    auto &node = builder_.getStmt(id);
    switch (ast::stmtKind(node)) {
    case ast::StmtKind::Let: {
        const auto &n = std::get<ast::LetNode>(node);
        if (n.init != ast::kInvalidExpr)
            resolveExpr(n.init);
        break;
    }
    case ast::StmtKind::Assign: {
        const auto &n = std::get<ast::AssignNode>(node);
        resolveExpr(n.target);
        resolveExpr(n.value);
        break;
    }
    case ast::StmtKind::Return: {
        const auto &n = std::get<ast::RetNode>(node);
        if (n.value != ast::kInvalidExpr)
            resolveExpr(n.value);
        break;
    }
    case ast::StmtKind::Goto:
        break;
    case ast::StmtKind::Marker:
        for (auto stmt_id : std::get<ast::MarkerNode>(node).body)
            resolveStmt(stmt_id);
        break;
    case ast::StmtKind::Expr:
        resolveExpr(std::get<ast::ExprStmtNode>(node).expr);
        break;
    }
}

void Resolver::resolveExpr(ast::ExprId id) {
    if (id == ast::kInvalidExpr)
        return;

    auto &node = builder_.getExpr(id);
    switch (ast::exprKind(node)) {
    case ast::ExprKind::Literal:
    case ast::ExprKind::Unbody:
        break;
    case ast::ExprKind::Identifier: {
        const auto &n = std::get<ast::IdentNode>(node);
        SymId sym     = n.scope_escape ? syms_.lookupEscaped(n.name) : lookupQualified(n.name);
        if (sym == kInvalidSym) {
            auto dot = n.name.find('.');
            if (dot == std::string_view::npos) {
                diags_.report(diagnostics::Severity::Error, diagnostics::err::UndefinedIdent,
                              common::format("undefined identifier '%.*s'",
                                             static_cast<int>(n.name.size()), n.name.data()),
                              {});
            }
        }
        setResolved(id, sym);
        break;
    }
    case ast::ExprKind::Binary: {
        const auto &n = std::get<ast::BinaryNode>(node);
        resolveExpr(n.lhs);
        resolveExpr(n.rhs);
        break;
    }
    case ast::ExprKind::Unary:
        resolveExpr(std::get<ast::UnaryNode>(node).operand);
        break;
    case ast::ExprKind::Call: {
        const auto &n = std::get<ast::CallNode>(node);
        resolveExpr(n.callee);
        for (auto arg : n.args)
            resolveExpr(arg);
        break;
    }
    case ast::ExprKind::Block: {
        const auto &n = std::get<ast::BlockNode>(node);
        syms_.enterScope();
        for (auto stmt_id : n.stmts)
            resolveStmt(stmt_id);
        if (n.trailing != ast::kInvalidExpr)
            resolveExpr(n.trailing);
        syms_.exitScope();
        break;
    }
    case ast::ExprKind::If: {
        const auto &n = std::get<ast::IfNode>(node);
        resolveExpr(n.cond);
        resolveExpr(n.then_branch);
        if (n.else_branch != ast::kInvalidExpr)
            resolveExpr(n.else_branch);
        break;
    }
    case ast::ExprKind::While: {
        const auto &n = std::get<ast::WhileNode>(node);
        resolveExpr(n.cond);
        resolveExpr(n.body);
        break;
    }
    case ast::ExprKind::Field:
        resolveExpr(std::get<ast::FieldNode>(node).object);
        break;
    case ast::ExprKind::Index: {
        const auto &n = std::get<ast::IndexNode>(node);
        resolveExpr(n.object);
        resolveExpr(n.index);
        break;
    }
    case ast::ExprKind::Range: {
        const auto &n = std::get<ast::RangeNode>(node);
        resolveExpr(n.lhs);
        resolveExpr(n.rhs);
        break;
    }
    case ast::ExprKind::Intrinsic:
        for (auto arg : std::get<ast::IntrinsicNode>(node).args)
            resolveExpr(arg);
        break;
    case ast::ExprKind::MacroCall:
        for (auto arg : std::get<ast::MacroCallNode>(node).args)
            resolveExpr(arg);
        break;
    }
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
                      common::format("undefined identifier '%.*s'", static_cast<int>(first.size()),
                                     first.data()),
                      {});
        return kInvalidSym;
    }

    sym = followAliases(sym);
    if (sym == kInvalidSym)
        return kInvalidSym;

    auto &data = syms_.get(sym);
    if (!isNamespaceLike(data.kind)) {
        diags_.report(diagnostics::Severity::Error, diagnostics::err::NotNamespace,
                      common::format("'%.*s' is not a namespace, so '%.*s' is invalid",
                                     static_cast<int>(first.size()), first.data(),
                                     static_cast<int>(name.size()), name.data()),
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
                          common::format("no member '%.*s' in '%s'",
                                         static_cast<int>(segment.size()), segment.data(),
                                         prefix.c_str()),
                          {});
            return kInvalidSym;
        }
        found = followAliases(found);
        if (found == kInvalidSym)
            return kInvalidSym;
        if (next_dot == std::string_view::npos)
            return found;
        auto &next_data = syms_.get(found);
        if (!isNamespaceLike(next_data.kind)) {
            auto current = common::format("%s.%.*s", prefix.c_str(),
                                          static_cast<int>(segment.size()), segment.data());
            diags_.report(
                diagnostics::Severity::Error, diagnostics::err::NotNamespace,
                common::format("'%s' is not a namespace, so the remainder of '%.*s' cannot be "
                               "resolved",
                               current.c_str(), static_cast<int>(name.size()), name.data()),
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
