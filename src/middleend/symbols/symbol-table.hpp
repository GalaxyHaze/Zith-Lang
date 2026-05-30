#pragma once

#include "infra/alloc/arena.hpp"
#include "infra/collections/dyn-array.hpp"
#include "infra/interner/string-interner.hpp"
#include "middleend/symbols/symbol-id.hpp"
#include <string_view>

namespace zith::middleend::symbols {

    struct Scope {
        ScopeId parent = kInvalidScope;
        infra::collections::DynArray<SymId> syms;
    };

    class SymbolTable {
        infra::alloc::Arena* arena_;
        infra::collections::DynArray<Scope> scopes_;
        ScopeId current_ = kRootScope;

    public:
        explicit SymbolTable(infra::alloc::Arena& arena);

        ScopeId enterScope();
        void exitScope();
        ScopeId currentScope() const noexcept;

        SymId declare(std::string_view name);
        SymId lookup(std::string_view name) const;
        SymId lookupInScope(std::string_view name, ScopeId scope) const;
    };

}
