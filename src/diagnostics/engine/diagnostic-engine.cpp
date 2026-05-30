#include "diagnostic-engine.hpp"

namespace zith::diagnostics::engine {

    void DiagnosticEngine::report(model::Diagnostic diag) {
        diagnostics_.push_back(std::move(diag));
    }

    void DiagnosticEngine::report(model::Severity sev, uint32_t code,
                                    std::string msg, frontend::Span span) {
        diagnostics_.push_back({sev, code, std::move(msg), span, {}});
    }

    bool DiagnosticEngine::hasErrors() const noexcept {
        for (auto& d : diagnostics_)
            if (d.isError()) return true;
        return false;
    }

    std::span<const model::Diagnostic> DiagnosticEngine::all() const noexcept {
        return diagnostics_;
    }

    void DiagnosticEngine::clear() {
        diagnostics_.clear();
    }

}
