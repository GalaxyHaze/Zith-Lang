#include "diagnostic-engine.hpp"

namespace zith::diagnostics {

void DiagnosticEngine::report(Diagnostic diag) {
    diagnostics_.push(std::move(diag));
}

void DiagnosticEngine::report(Severity sev, uint32_t code, std::string msg, memory::Span span) {
    diagnostics_.emplace(diagnostics_.arena(), sev, code, std::move(msg), span);
}

bool DiagnosticEngine::hasErrors() const noexcept {
    for (auto &d : diagnostics_)
        if (d.isError())
            return true;
    return false;
}

std::span<const Diagnostic> DiagnosticEngine::all() const noexcept {
    return {diagnostics_.data(), diagnostics_.size()};
}

void DiagnosticEngine::clear() {
    diagnostics_.clear();
}

} // namespace zith::diagnostics
