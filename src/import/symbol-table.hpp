#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/string-interner.hpp"
#include "import/symbol-id.hpp"

#include <string_view>

namespace zith::import {

    struct Scope {
        ScopeId parent = kInvalidScope;
        memory::DynArray<SymId> syms;
    };

    class SymbolTable {
        memory::Arena *arena_;
        memory::DynArray<Scope> scopes_;
        ScopeId current_ = kRootScope;

    public:
        explicit SymbolTable(memory::Arena &arena);

        ScopeId enterScope();
        void exitScope();
        ScopeId currentScope() const noexcept;

        SymId declare(std::string_view name);
        SymId lookup(std::string_view name) const;
        SymId lookupInScope(std::string_view name, ScopeId scope) const;
    };

} // namespace zith::import
