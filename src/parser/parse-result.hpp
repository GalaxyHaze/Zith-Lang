#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "ast/ast-ids.hpp"


namespace zith::parser {

    template <typename T> struct ParseResult {
        T value;
        diagnostics::DiagnosticEngine diagnostics;
        bool ok = true;

        explicit operator bool() const noexcept {
            return ok;
        }
    };

    using ProgramResult = ParseResult<ast::DeclId>;

} // namespace zith::parser
