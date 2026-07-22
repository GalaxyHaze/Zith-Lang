#include "diagnostic-engine.hpp"

namespace zith::diagnostics {

void DiagnosticEngine::report(Diagnostic diag) {
    diagnostics_.push(std::move(diag));
}

void DiagnosticEngine::reportError(uint32_t code, std::string msg, memory::Span span) {
    report(Severity::Error, code, std::move(msg), span);
}

void DiagnosticEngine::reportError(uint32_t code, std::string msg, memory::Span span,
                                   std::initializer_list<std::string> suggestions) {
    report(Severity::Error, code, std::move(msg), span, suggestions);
}

void DiagnosticEngine::report(Severity sev, uint32_t code, std::string msg, memory::Span span) {
    diagnostics_.emplace(diagnostics_.getArena(), sev, code, std::move(msg), span);
}

void DiagnosticEngine::report(Severity sev, uint32_t code, std::string msg, memory::Span span,
                              std::initializer_list<std::string> suggestions) {
    Diagnostic diag(diagnostics_.getArena(), sev, code, std::move(msg), span);
    for (auto &s : suggestions)
        diag.suggestions.push(s);
    diagnostics_.push(std::move(diag));
}

bool DiagnosticEngine::hasErrors() const noexcept {
    for (auto &d : diagnostics_)
        if (d.isError())
            return true;
    return false;
}

size_t DiagnosticEngine::errorCount() const noexcept {
    size_t count = 0;
    for (auto &d : diagnostics_)
        if (d.isError())
            ++count;
    return count;
}

std::span<const Diagnostic> DiagnosticEngine::all() const noexcept {
    return {diagnostics_.getData(), diagnostics_.size()};
}

void DiagnosticEngine::clear() {
    diagnostics_.clear();
}

} // namespace zith::diagnostics
