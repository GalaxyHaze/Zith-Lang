#include "symbol-table.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "ast/type-expr.hpp"

namespace zith::symbols {

SymbolTable::SymbolTable(memory::Arena &arena, memory::StringInterner *interner)
    : arena_(&arena), interner_(interner), scopes_(arena), symbols_(arena) {
    scopes_.emplace(Scope{kInvalidScope, memory::DynArray<SymId>(arena)});
    current_ = kRootScope;
}

ScopeId SymbolTable::enterScope() {
    ScopeId id = static_cast<ScopeId>(scopes_.size());
    scopes_.emplace(Scope{current_, memory::DynArray<SymId>(*arena_)});
    current_ = id;
    return id;
}

void SymbolTable::exitScope() {
    if (current_ != kRootScope) {
        current_ = scopes_[current_].parent;
    }
}

ScopeId SymbolTable::currentScope() const noexcept {
    return current_;
}

SymId SymbolTable::declare(memory::InternedId name, SymbolVisibility vis, int32_t depth, SymKind kind,
                           ast::DeclId decl_id, memory::Span span, SymId target,
                           memory::Span doc_span) {
    SymId id = static_cast<SymId>(symbols_.size());
    symbols_.push(
        SymbolData{name, current_, vis, depth, kind, decl_id, span, doc_span, target, *arena_});
    scopes_[current_].syms.push(id);
    return id;
}

SymId SymbolTable::declareInScope(ScopeId scope, memory::InternedId name, SymbolVisibility vis,
                                  int32_t depth, SymKind kind, ast::DeclId decl_id,
                                  memory::Span span, SymId target, memory::Span doc_span) {
    SymId id = static_cast<SymId>(symbols_.size());
    symbols_.push(
        SymbolData{name, scope, vis, depth, kind, decl_id, span, doc_span, target, *arena_});
    scopes_[scope].syms.push(id);
    return id;
}

SymId SymbolTable::declare(std::string_view name, SymbolVisibility vis, int32_t depth, SymKind kind,
                           ast::DeclId decl_id, memory::Span span, SymId target,
                           memory::Span doc_span) {
    return declare(interner_->intern(name), vis, depth, kind, decl_id, span, target, doc_span);
}

SymId SymbolTable::declareInScope(ScopeId scope, std::string_view name, SymbolVisibility vis,
                                  int32_t depth, SymKind kind, ast::DeclId decl_id,
                                  memory::Span span, SymId target, memory::Span doc_span) {
    return declareInScope(scope, interner_->intern(name), vis, depth, kind, decl_id, span, target,
                          doc_span);
}

SymbolData &SymbolTable::get(SymId id) {
    return symbols_[id];
}

SymId SymbolTable::lookup(memory::InternedId name) const {
    ScopeId scope = current_;
    while (scope != kInvalidScope) {
        for (auto sid : scopes_[scope].syms) {
            if (symbols_[sid].name == name)
                return sid;
        }
        scope = scopes_[scope].parent;
    }
    return kInvalidSym;
}

SymId SymbolTable::lookupInScope(memory::InternedId name, ScopeId scope) const {
    for (auto sid : scopes_[scope].syms) {
        if (symbols_[sid].name == name)
            return sid;
    }
    return kInvalidSym;
}

SymId SymbolTable::lookup(std::string_view name) const {
    return lookup(interner_->intern(name));
}

SymId SymbolTable::lookupInScope(std::string_view name, ScopeId scope) const {
    return lookupInScope(interner_->intern(name), scope);
}

memory::DynArray<SymId> SymbolTable::lookupAll(std::string_view name, memory::Arena &arena) const {
    return lookupAll(interner_->intern(name), arena);
}

memory::DynArray<SymId> SymbolTable::lookupAll(memory::InternedId name, memory::Arena &arena) const {
    memory::DynArray<SymId> result{arena};
    ScopeId scope = current_;
    while (scope != kInvalidScope) {
        for (auto sid : scopes_[scope].syms) {
            if (symbols_[sid].name == name)
                result.push(sid);
        }
        scope = scopes_[scope].parent;
    }
    return result;
}

const SymbolData &SymbolTable::get(SymId id) const {
    return symbols_[id];
}

size_t SymbolTable::symbolCount() const noexcept {
    return symbols_.size();
}

ScopeId SymbolTable::scopeCount() const noexcept {
    return static_cast<ScopeId>(scopes_.size());
}

static const char *symKindName(SymKind k) {
    switch (k) {
    case SymKind::Fn:
        return "fn";
    case SymKind::Struct:
        return "struct";
    case SymKind::Trait:
        return "trait";
    case SymKind::Enum:
        return "enum";
    case SymKind::Alias:
        return "alias";
    case SymKind::Variable:
        return "var";
    case SymKind::Module:
        return "mod";
    case SymKind::Component:
        return "comp";
    case SymKind::Union:
        return "union";
    case SymKind::Interface:
        return "interface";
    case SymKind::Asset:
        return "asset";
    }
    return "?";
}

void SymbolTable::dump(FILE *out, ast::AstBuilder *bld) const {
    std::fprintf(out, "SymbolTable (%zu symbols, %zu scopes):\n", symbols_.size(), scopes_.size());
    for (ScopeId s = 0; s < static_cast<ScopeId>(scopes_.size()); ++s) {
        auto &scope = scopes_[s];
        if (scope.parent == kInvalidScope)
            std::fprintf(out, "  Scope %u (root)\n", s);
        else
            std::fprintf(out, "  Scope %u (parent %u)\n", s, scope.parent);
        for (auto sid : scope.syms) {
            auto &sym = symbols_[sid];
            auto vis  = sym.visibility == SymbolVisibility::Public   ? "pub"
                        : sym.visibility == SymbolVisibility::Module ? "mod"
                                                                     : "priv";
            auto sym_name = interner_->lookup(sym.name);
            std::fprintf(out, "    [%u] %s %s %.*s", sid, vis, symKindName(sym.kind),
                         (int)sym_name.size(), sym_name.data());
            if (sym.visibility == SymbolVisibility::Module)
                std::fprintf(out, " (depth=%d)", sym.mod_depth);
            switch (sym.kind) {
            case SymKind::Fn:
                std::fprintf(out, " (%zu arguments)", sym.members.size());
                break;
            case SymKind::Variable:
                std::fprintf(out, " (primitivo)");
                break;
            case SymKind::Struct:
            case SymKind::Enum:
            case SymKind::Union: {
                size_t methods = 0;
                for (auto mid : sym.members)
                    if (symbols_[mid].kind == SymKind::Fn) methods++;
                size_t fields = 0;
                if (bld && sym.decl_id != ast::kInvalidDecl) {
                    auto &decl = bld->getDecl(sym.decl_id);
                    if (auto *sn = std::get_if<ast::StructDeclNode>(&decl))
                        fields = sn->fields.size();
                    else if (auto *en = std::get_if<ast::EnumDeclNode>(&decl))
                        fields = en->variants.size();
                    else if (auto *un = std::get_if<ast::UnionDeclNode>(&decl))
                        fields = un->variants.size();
                }
                std::fprintf(out, " (%zu fields & %zu methods)", fields, methods);
                break;
            }
            default:
                std::fprintf(out, " (%zu members)", sym.members.size());
                break;
            }
            std::fprintf(out, "\n");
        }
    }
}

} // namespace zith::symbols
