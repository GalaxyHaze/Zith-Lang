#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-builder.hpp"
#include "import/symbol-table.hpp"
#include "types/type-intern.hpp"

namespace zith::middleend::sema {

    class SemaContext {
        symbols::SymbolTable &syms_;
        types::TypeIntern &types_;
        diagnostics::engine::DiagnosticEngine &diags_;
        frontend::ast::AstBuilder &builder_;
        types::TypeId current_fn_ret_ = types::kVoidType;

    public:
        SemaContext(symbols::SymbolTable &syms,
                    types::TypeIntern &types,
                    diagnostics::engine::DiagnosticEngine &diags,
                    frontend::ast::AstBuilder &builder);

        symbols::SymbolTable &syms() noexcept;
        types::TypeIntern &types() noexcept;
        diagnostics::engine::DiagnosticEngine &diags() noexcept;
        frontend::ast::AstBuilder &builder() noexcept;

        types::TypeId currentFnRet() const noexcept;
        void setCurrentFnRet(types::TypeId t) noexcept;
    };

} // namespace zith::middleend::sema
