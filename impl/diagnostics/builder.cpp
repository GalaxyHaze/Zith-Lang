#include "diagnostics/builder.hpp"
#include "diagnostics/diagnostic_bag.hpp"

#include <utility>

namespace zith::diag {

DiagnosticBuilder::DiagnosticBuilder(DiagLevel level, DiagCode code)
    : diag_(std::make_unique<Diagnostic>()) {
    diag_->level = level;
    diag_->code = code;
}

DiagnosticBuilder::DiagnosticBuilder(DiagLevel level, DiagCode code, SourceSpan span)
    : DiagnosticBuilder(level, code) {
    with_span(span);
}

DiagnosticBuilder& DiagnosticBuilder::with_raw_message(std::string msg) {
    diag_->message = LazyMessage(std::move(msg));
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::with_message_lazy(LazyMessage msg) {
    diag_->message = std::move(msg);
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::with_span(SourceSpan span) {
    diag_->primary_span = span;
    diag_->has_primary_span = true;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::with_suggestion(SourceSpan span, std::string replacement,
                                                       std::string label, bool auto_applicable) {
    Suggestion sug;
    sug.span = span;
    sug.replacement = std::move(replacement);
    sug.label = std::move(label);
    sug.is_machine_applicable = auto_applicable;
    diag_->suggestions.push_back(std::move(sug));
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::with_cause(DiagCode caused_by) {
    diag_->caused_by = caused_by;
    return *this;
}

DiagnosticBuilder& DiagnosticBuilder::with_child(std::unique_ptr<Diagnostic> child) {
    diag_->children.push_back(std::move(child));
    return *this;
}

bool DiagnosticBuilder::emit(DiagnosticBag& bag) {
    if (!diag_) return false;
    Diagnostic d = std::move(*diag_);
    diag_.reset();
    bag.emit(std::move(d));
    return d.is_error();
}

Diagnostic DiagnosticBuilder::take() {
    if (!diag_) return {};
    return std::move(*diag_);
}

} // namespace zith::diag
