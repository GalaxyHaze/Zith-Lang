#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/hir/hir-module.hpp"
#include "import/symbol-table.hpp"

namespace zith::sema {

    struct SemaResult {
        zir::hir::HirModule hir;
        diagnostics::DiagnosticEngine diagnostics;
        import::SymbolTable symbols;

        SemaResult(zir::hir::HirModule hir, diagnostics::DiagnosticEngine diagnostics, import::SymbolTable symbols) :
            hir(std::move(hir)), diagnostics(std::move(diagnostics)), symbols(std::move(symbols)) {}
    };

} // namespace zith::sema
