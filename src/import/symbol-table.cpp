#include "symbol-table.hpp"

namespace zith::import {

    SymbolTable::SymbolTable(memory::Arena &arena) : arena_(&arena), scopes_(arena) {
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

    SymId SymbolTable::declare(std::string_view name) {
        (void)name;
        return 0;
    }

    SymId SymbolTable::lookup(std::string_view name) const {
        (void)name;
        return kInvalidSym;
    }

    SymId SymbolTable::lookupInScope(std::string_view name, ScopeId scope) const {
        (void)name;
        (void)scope;
        return kInvalidSym;
    }

} // namespace zith::import
