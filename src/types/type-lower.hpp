#pragma once

#include "ast/ast-builder.hpp"
#include "ast/type-expr.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "types/type-intern.hpp"

#include <string_view>
#include <unordered_map>

namespace zith::types {

/// Lowers TypeExprNodes (from the AST) into concrete TypeIds.
/// Requires access to AstBuilder for reading sub-expressions,
/// TypeIntern for creating interned types, and Diagnostics for errors.
class TypeLower {
    ast::AstBuilder &ast_;
    TypeIntern &intern_;
    diagnostics::DiagnosticEngine &diags_;
    const std::unordered_map<std::string_view, TypeId> *generic_ctx_ = nullptr;

public:
    TypeLower(ast::AstBuilder &ast, TypeIntern &intern, diagnostics::DiagnosticEngine &diags);

    void setGenericContext(const std::unordered_map<std::string_view, TypeId> *ctx);

    /// Lower a type expression by its ID (reads from AstBuilder).
    TypeId lower(ast::TypeExprId id);

    /// Lower a type expression node directly.
    TypeId lowerNode(const ast::TypeExprNode &node);
};

} // namespace zith::types
