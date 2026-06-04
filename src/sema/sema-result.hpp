#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/hir/hir-module.hpp"
#include "import/symbol-table.hpp"

namespace zith::sema {

    struct SemaResult {
        zir::hir::HirModule hir;
        diagnostics::DiagnosticEngine diagnostics;
        import::SymbolTable symbols;
    };

} // namespace zith::sema
