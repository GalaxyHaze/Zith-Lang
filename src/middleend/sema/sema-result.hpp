#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "middleend/hir/hir-module.hpp"
#include "middleend/symbols/symbol-table.hpp"

namespace zith::middleend::sema {

    struct SemaResult {
        hir::HirModule hir;
        diagnostics::engine::DiagnosticEngine diagnostics;
        symbols::SymbolTable symbols;
    };

}
