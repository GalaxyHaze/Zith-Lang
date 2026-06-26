#pragma once

#include "ast/ast-ids.hpp"
#include "import/symbol-id.hpp"
#include "import/symbol-visibility.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/span.hpp"

#include <cstdio>
#include <string_view>

namespace zith::import {

enum class SymKind : uint8_t {
    Fn,
    Struct,
    Trait,
    Interface,
    Enum,
    Alias,
    Variable,
    Module,
    Component,
    Union,
};

struct SymbolData {
    std::string_view name;
    ScopeId scope;
    SymbolVisibility visibility = SymbolVisibility::Private;
    int32_t mod_depth           = 0;
    SymKind kind                = SymKind::Variable;
    ast::DeclId decl_id         = ast::kInvalidDecl;
    memory::Span span{};
    memory::Span doc_span{};
    SymId target = kInvalidSym;
    memory::DynArray<SymId> members;

    SymbolData(std::string_view name, ScopeId scope, SymbolVisibility vis, int32_t depth,
               SymKind ikind, ast::DeclId did, memory::Span ispan, memory::Span idoc, SymId itarget,
               memory::Arena &arena)
        : name(name), scope(scope), visibility(vis), mod_depth(depth), kind(ikind), decl_id(did),
          span(ispan), doc_span(idoc), target(itarget), members(arena) {}
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

    SymId declare(std::string_view name, SymbolVisibility vis = SymbolVisibility::Private,
                  int32_t depth = 0, SymKind kind = SymKind::Variable,
                  ast::DeclId decl_id = ast::kInvalidDecl, memory::Span span = {},
                  SymId target = kInvalidSym, memory::Span doc_span = {});
    SymId declareInScope(ScopeId scope, std::string_view name,
                         SymbolVisibility vis = SymbolVisibility::Private, int32_t depth = 0,
                         SymKind kind = SymKind::Variable, ast::DeclId decl_id = ast::kInvalidDecl,
                         memory::Span span = {}, SymId target = kInvalidSym,
                         memory::Span doc_span = {});
    SymId lookup(std::string_view name) const;
    SymId lookupInScope(std::string_view name, ScopeId scope) const;
    memory::DynArray<SymId> lookupAll(std::string_view name, memory::Arena &arena) const;

    SymbolData &get(SymId id);
    const SymbolData &get(SymId id) const;
    size_t symbolCount() const noexcept;
    ScopeId scopeCount() const noexcept;

    void dump(FILE *out = stdout) const;
};

} // namespace zith::import
