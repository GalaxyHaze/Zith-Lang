#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "symbols/symbol-table.hpp"
#include "hir/hir-module.hpp"

namespace zith::sema {

struct SemaResult {
    hir::HirModule hir;
    symbols::SymbolTable symbols;

    SemaResult(hir::HirModule hir, diagnostics::DiagnosticEngine diagnostics,
               symbols::SymbolTable symbols)
        : hir(std::move(hir)), diagnostics(std::move(diagnostics)), symbols(std::move(symbols)) {}
};

} // namespace zith::sema
