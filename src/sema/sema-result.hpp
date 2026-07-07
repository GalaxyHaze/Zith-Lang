#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"
#include "symbols/symbol-table.hpp"

namespace zith::sema {

struct SemaResult {
    hir::HirModule hir;
    symbols::SymbolTable symbols;

    SemaResult(hir::HirModule hir, symbols::SymbolTable symbols)
        : hir(std::move(hir)), symbols(std::move(symbols)) {}
};

} // namespace zith::sema
