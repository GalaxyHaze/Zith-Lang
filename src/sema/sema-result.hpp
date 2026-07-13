#pragma once

#include "sema/typed-ast.hpp"
#include "symbols/symbol-table.hpp"

namespace zith::sema {

struct SemaResult {
    TypedAst typed_ast;
    symbols::SymbolTable symbols;

    SemaResult(TypedAst typed_ast_, symbols::SymbolTable symbols_)
        : typed_ast(std::move(typed_ast_)), symbols(std::move(symbols_)) {}
};

} // namespace zith::sema
