#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "frontend/ast/ast-ids.hpp"
#include "infra/util/result.hpp"

#include <variant>

namespace zith::frontend::parser {

    template <typename T> struct ParseResult {
        T value;
        diagnostics::engine::DiagnosticEngine diagnostics;
        bool ok = true;

        explicit operator bool() const noexcept {
            return ok;
        }
    };

    using ProgramResult = ParseResult<ast::DeclId>;

} // namespace zith::frontend::parser
