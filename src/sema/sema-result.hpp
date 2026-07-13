#pragma once

#include "hir/hir-module.hpp"
#include "symbols/symbol-table.hpp"

namespace zith::sema {

struct SemaResult {
    hir::HirModule hir;
    symbols::SymbolTable symbols;

    SemaResult(hir::HirModule hir_, symbols::SymbolTable symbols_)
        : hir(std::move(hir_)), symbols(std::move(symbols_)) {}
};

} // namespace zith::sema
