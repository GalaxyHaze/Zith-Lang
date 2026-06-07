#pragma once

#include "diagnostic.hpp"
#include "diagnostics/diagnostic.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace zith::diagnostics {

    class DiagnosticEngine {
        std::vector<Diagnostic> diagnostics_;

    public:
        void report(Diagnostic diag);
        void report(Severity sev, uint32_t code, std::string msg, parser::Span span);

        void emit() const;
        void emitTo(std::string_view source_text) const;

        [[nodiscard]] bool hasErrors() const noexcept;
        [[nodiscard]] std::span<const Diagnostic> all() const noexcept;
        void clear();
    };

} // namespace zith::diagnostics
