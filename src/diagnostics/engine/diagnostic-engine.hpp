#pragma once

#include "diagnostics/model/diagnostic.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace zith::diagnostics::engine {

    class DiagnosticEngine {
        std::vector<model::Diagnostic> diagnostics_;

    public:
        void report(model::Diagnostic diag);
        void report(model::Severity sev, uint32_t code, std::string msg, frontend::Span span);

        [[nodiscard]] bool hasErrors() const noexcept;
        [[nodiscard]] std::span<const model::Diagnostic> all() const noexcept;
        void clear();
    };

} // namespace zith::diagnostics::engine
