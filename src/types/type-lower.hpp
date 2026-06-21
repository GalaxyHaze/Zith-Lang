#pragma once

#include "ast/ast-builder.hpp"
#include "ast/type-expr.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/symbol-table.hpp"
#include "memory/flat_map.hpp"
#include "types/type-intern.hpp"

#include <string_view>

namespace zith::types {

/// Lowers TypeExprNodes (from the AST) into concrete TypeIds.
/// Requires access to AstBuilder for reading sub-expressions,
/// TypeIntern for creating interned types, SymbolTable for resolving
/// named types, and Diagnostics for errors.
class TypeLower {
    ast::AstBuilder &ast_;
    TypeIntern &intern_;
    diagnostics::DiagnosticEngine &diags_;
    const import::SymbolTable &syms_;
    const memory::FlatMap<std::string_view, TypeId> *generic_ctx_ = nullptr;

public:
    TypeLower(ast::AstBuilder &ast, TypeIntern &intern,
              diagnostics::DiagnosticEngine &diags,
              const import::SymbolTable &syms);

    void setGenericContext(const memory::FlatMap<std::string_view, TypeId> *ctx);

    /// Lower a type expression by its ID (reads from AstBuilder).
    TypeId lower(ast::TypeExprId id);

    /// Lower a type expression node directly.
    TypeId lowerNode(const ast::TypeExprNode &node);
};

} // namespace zith::types
