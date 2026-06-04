#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "zir/hir/hir-module.hpp"
#include "import/symbol-table.hpp"

namespace zith::middleend::sema {

    struct SemaResult {
        hir::HirModule hir;
        diagnostics::engine::DiagnosticEngine diagnostics;
        symbols::SymbolTable symbols;
    };

} // namespace zith::middleend::sema
