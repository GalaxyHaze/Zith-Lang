#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/string-interner.hpp"
#include "import/symbol-id.hpp"

#include <string_view>

namespace zith::middleend::symbols {

    struct Scope {
        ScopeId parent = kInvalidScope;
        infra::collections::DynArray<SymId> syms;
    };

    class SymbolTable {
        infra::alloc::Arena *arena_;
        infra::collections::DynArray<Scope> scopes_;
        ScopeId current_ = kRootScope;

    public:
        explicit SymbolTable(infra::alloc::Arena &arena);

        ScopeId enterScope();
        void exitScope();
        ScopeId currentScope() const noexcept;

        SymId declare(std::string_view name);
        SymId lookup(std::string_view name) const;
        SymId lookupInScope(std::string_view name, ScopeId scope) const;
    };

} // namespace zith::middleend::symbols
