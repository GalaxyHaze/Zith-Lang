#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "frontend/ast/ast-builder.hpp"
#include "middleend/symbols/symbol-table.hpp"
#include "middleend/types/type-intern.hpp"

namespace zith::middleend::sema {

    class SemaContext {
        symbols::SymbolTable& syms_;
        types::TypeIntern& types_;
        diagnostics::engine::DiagnosticEngine& diags_;
        frontend::ast::AstBuilder& builder_;
        types::TypeId current_fn_ret_ = types::kVoidType;

    public:
        SemaContext(symbols::SymbolTable& syms, types::TypeIntern& types,
                    diagnostics::engine::DiagnosticEngine& diags,
                    frontend::ast::AstBuilder& builder);

        symbols::SymbolTable& syms() noexcept;
        types::TypeIntern& types() noexcept;
        diagnostics::engine::DiagnosticEngine& diags() noexcept;
        frontend::ast::AstBuilder& builder() noexcept;

        types::TypeId currentFnRet() const noexcept;
        void setCurrentFnRet(types::TypeId t) noexcept;
    };

}
