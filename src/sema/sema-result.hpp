#pragma once

#include "hir/hir-module.hpp"
#include "sema/typed-ast.hpp"
#include "symbols/symbol-table.hpp"

namespace zith::sema {

struct SemaResult {
    hir::HirModule hir;
    TypedAst typed_ast;
    symbols::SymbolTable symbols;

    SemaResult(hir::HirModule hir_, TypedAst typed_ast_, symbols::SymbolTable symbols_)
        : hir(std::move(hir_)), typed_ast(std::move(typed_ast_)), symbols(std::move(symbols_)) {}
};

} // namespace zith::sema
