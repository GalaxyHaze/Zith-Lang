#pragma once

#include "diagnostics/diagnostic.hpp"
#include "diagnostics/format.hpp"

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace zith::diag {

class DiagnosticBag;

class DiagnosticBuilder {
public:
    DiagnosticBuilder(DiagLevel level, DiagCode code);
    DiagnosticBuilder(DiagLevel level, DiagCode code, SourceSpan span);

    DiagnosticBuilder(DiagnosticBuilder&&) noexcept = default;
    DiagnosticBuilder& operator=(DiagnosticBuilder&&) noexcept = default;

    template <typename... Args>
    DiagnosticBuilder& with_message(std::string_view fmt, Args&&... args) {
        diag_->message = LazyMessage(diag_format(fmt, std::forward<Args>(args)...));
        return *this;
    }

    DiagnosticBuilder& with_raw_message(std::string msg);

    DiagnosticBuilder& with_message_lazy(LazyMessage msg);

    DiagnosticBuilder& with_span(SourceSpan span);

    template <typename... Args>
    DiagnosticBuilder& with_secondary_span(SourceSpan span, std::string_view label_fmt, Args&&... args) {
        LabeledSpan ls;
        ls.span = span;
        ls.label = LazyMessage(diag_format(label_fmt, std::forward<Args>(args)...));
        diag_->secondary_spans.push_back(std::move(ls));
        return *this;
    }

    DiagnosticBuilder& with_suggestion(SourceSpan span, std::string replacement,
                                        std::string label, bool auto_applicable = false);

    template <typename... Args>
    DiagnosticBuilder& with_note(std::string_view fmt, Args&&... args) {
        auto note_diag = std::make_unique<Diagnostic>();
        note_diag->level = DiagLevel::Note;
        note_diag->code = diag_->code;
        note_diag->message = LazyMessage(diag_format(fmt, std::forward<Args>(args)...));
        diag_->children.push_back(std::move(note_diag));
        return *this;
    }

    template <typename... Args>
    DiagnosticBuilder& with_help(std::string_view fmt, Args&&... args) {
        auto help_diag = std::make_unique<Diagnostic>();
        help_diag->level = DiagLevel::Help;
        help_diag->code = diag_->code;
        help_diag->message = LazyMessage(diag_format(fmt, std::forward<Args>(args)...));
        diag_->children.push_back(std::move(help_diag));
        return *this;
    }

    template <typename... Args>
    DiagnosticBuilder& with_hint(std::string_view fmt, Args&&... args) {
        auto hint_diag = std::make_unique<Diagnostic>();
        hint_diag->level = DiagLevel::Hint;
        hint_diag->code = diag_->code;
        hint_diag->message = LazyMessage(diag_format(fmt, std::forward<Args>(args)...));
        diag_->children.push_back(std::move(hint_diag));
        return *this;
    }

    DiagnosticBuilder& with_cause(DiagCode caused_by);
    DiagnosticBuilder& with_child(std::unique_ptr<Diagnostic> child);

    bool emit(DiagnosticBag& bag);
    Diagnostic take();

private:
    std::unique_ptr<Diagnostic> diag_;
};

} // namespace zith::diag
