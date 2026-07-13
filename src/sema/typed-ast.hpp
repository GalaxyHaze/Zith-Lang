#pragma once

#include "ast/ast-ids.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "types/type-id.hpp"

namespace zith::sema {

class TypedAst {
public:
    explicit TypedAst(memory::Arena &arena) : expr_types_(arena) {}

    void set(ast::ExprId id, types::TypeId type) {
        if (id == ast::kInvalidExpr)
            return;
        while (id >= expr_types_.size())
            expr_types_.push(types::kErrorType);
        expr_types_[id] = type;
    }

    types::TypeId get(ast::ExprId id) const {
        if (id == ast::kInvalidExpr || id >= expr_types_.size())
            return types::kErrorType;
        return expr_types_[id];
    }

    const memory::DynArray<types::TypeId> &exprTypes() const {
        return expr_types_;
    }

private:
    memory::DynArray<types::TypeId> expr_types_;
};

} // namespace zith::sema
