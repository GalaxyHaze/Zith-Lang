#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/string-interner.hpp"
#include "import/symbol-id.hpp"
#include "import/symbol-visibility.hpp"

#include <cstdio>
#include <string_view>

namespace zith::import {

    struct SymbolData {
        std::string_view name;
        ScopeId scope;
        SymbolVisibility visibility = SymbolVisibility::Private;
        int32_t mod_depth = 0;
    };

    struct Scope {
        ScopeId parent = kInvalidScope;
        memory::DynArray<SymId> syms;
    };

    class SymbolTable {
        memory::Arena *arena_;
        memory::DynArray<Scope> scopes_;
        memory::DynArray<SymbolData> symbols_;
        ScopeId current_ = kRootScope;

    public:
        explicit SymbolTable(memory::Arena &arena);

        ScopeId enterScope();
        void exitScope();
        ScopeId currentScope() const noexcept;

        SymId declare(std::string_view name, SymbolVisibility vis = SymbolVisibility::Private, int32_t depth = 0);
        SymId lookup(std::string_view name) const;
        SymId lookupInScope(std::string_view name, ScopeId scope) const;

        const SymbolData &get(SymId id) const;
        size_t symbolCount() const noexcept;
        ScopeId scopeCount() const noexcept;

        void dump(FILE *out = stdout) const;
    };

} // namespace zith::import
