#include "symbol-table.hpp"

namespace zith::import {

    SymbolTable::SymbolTable(memory::Arena &arena) :
        arena_(&arena), scopes_(arena), symbols_(arena) {
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

    SymId SymbolTable::declare(std::string_view name, SymbolVisibility vis, int32_t depth) {
        SymId id = static_cast<SymId>(symbols_.size());
        symbols_.push(SymbolData{name, current_, vis, depth});
        scopes_[current_].syms.push(id);
        return id;
    }

    SymId SymbolTable::lookup(std::string_view name) const {
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

    SymId SymbolTable::lookupInScope(std::string_view name, ScopeId scope) const {
        for (auto sid : scopes_[scope].syms) {
            if (symbols_[sid].name == name)
                return sid;
        }
        return kInvalidSym;
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

    void SymbolTable::dump(FILE *out) const {
        std::fprintf(out, "SymbolTable (%zu symbols, %zu scopes):\n",
                     symbols_.size(), scopes_.size());
        for (ScopeId s = 0; s < static_cast<ScopeId>(scopes_.size()); ++s) {
            auto &scope = scopes_[s];
            if (scope.parent == kInvalidScope)
                std::fprintf(out, "  Scope %u (root)\n", s);
            else
                std::fprintf(out, "  Scope %u (parent %u)\n", s, scope.parent);
            for (auto sid : scope.syms) {
                auto &sym = symbols_[sid];
                auto vis = sym.visibility == SymbolVisibility::Public ? "pub" :
                           sym.visibility == SymbolVisibility::Module ? "mod" : "priv";
                std::fprintf(out, "    [%u] %s %.*s",
                             sid, vis,
                             (int)sym.name.size(),
                             sym.name.data());
                if (sym.visibility == SymbolVisibility::Module)
                    std::fprintf(out, " (depth=%d)", sym.mod_depth);
                std::fprintf(out, "\n");
            }
        }
    }

} // namespace zith::import
