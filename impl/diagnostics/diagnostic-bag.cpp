#include "diagnostics/diagnostic_bag.hpp"
#include "diagnostics/format.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>

namespace zith::diag {

void DiagnosticBag::emit(Diagnostic diag) {
    if (diagnostics_.size() >= MAX_DIAGNOSTICS) return;

    if (diag.is_error()) ++error_count_;
    if (diag.level == DiagLevel::Fatal) ++fatal_count_;
    if (diag.is_warning()) ++warning_count_;

    diagnostics_.push_back(std::move(diag));
    finalized_ = false;
}

void DiagnosticBag::clear() {
    diagnostics_.clear();
    sorted_view_.clear();
    error_count_ = 0;
    fatal_count_ = 0;
    warning_count_ = 0;
    finalized_ = false;
}

DiagnosticBag::SpanIndex DiagnosticBag::get_span_key(const Diagnostic& d) const {
    SpanIndex idx;
    idx.file_id = d.primary_span.file_id;
    idx.start_line = d.primary_span.start.line;
    idx.start_col = d.primary_span.start.index;
    return idx;
}

void DiagnosticBag::suppress_cascading() {
    // Phase 1: Mark diagnostics that are caused_by an existing error.
    // If a diagnostic has `caused_by` set, and there exists an earlier
    // error with the matching code whose span encloses this diagnostic's span,
    // demote it to a note child of that error.
    std::vector<size_t> to_remove;
    std::vector<Diagnostic> promoted_notes;

    for (size_t i = 0; i < diagnostics_.size(); ++i) {
        auto& diag = diagnostics_[i];
        if (!diag.caused_by) continue;
        if (!diag.is_error()) continue;

        for (size_t j = 0; j < diagnostics_.size(); ++j) {
            if (i == j) continue;
            auto& parent = diagnostics_[j];
            if (!parent.is_error()) continue;
            if (parent.code != *diag.caused_by) continue;
            if (!parent.has_primary_span || !diag.has_primary_span) continue;
            if (!parent.primary_span.contains(diag.primary_span)) continue;

            // Demote: change to note, mark as child of parent
            auto note = std::make_unique<Diagnostic>();
            note->level = DiagLevel::Note;
            note->code = diag.code;
            note->message = std::move(diag.message);
            note->primary_span = diag.primary_span;
            note->has_primary_span = diag.has_primary_span;
            note->secondary_spans = std::move(diag.secondary_spans);
            parent.children.push_back(std::move(note));

            to_remove.push_back(i);
            if (diag.is_error() && error_count_ > 0) --error_count_;
            break;
        }
    }

    // Remove demoted diagnostics (in reverse order)
    std::sort(to_remove.begin(), to_remove.end());
    to_remove.erase(std::unique(to_remove.begin(), to_remove.end()), to_remove.end());
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
        diagnostics_.erase(diagnostics_.begin() + static_cast<ptrdiff_t>(*it));
    }
}

void DiagnosticBag::aggregate_duplicates() {
    // Group diagnostics by (code, message) within the same file.
    // If >3 identical errors at adjacent spans, collapse into one + count.
    std::map<std::pair<DiagCode, std::string>, std::vector<size_t>> groups;

    for (size_t i = 0; i < diagnostics_.size(); ++i) {
        auto& d = diagnostics_[i];
        if (!d.is_error()) continue;
        std::string msg = d.message.get();
        groups[{d.code, msg}].push_back(i);
    }

    std::vector<size_t> to_remove;
    for (auto& [key, indices] : groups) {
        if (indices.size() < 4) continue; // Only aggregate if >3

        // Keep the first one, add a note about repetition count
        auto& first = diagnostics_[indices[0]];
        auto repeat_note = std::make_unique<Diagnostic>();
        repeat_note->level = DiagLevel::Note;
        repeat_note->code = key.first;
        repeat_note->message = LazyMessage(
            diag_format("this error repeats {} more times", indices.size() - 1));
        first.children.push_back(std::move(repeat_note));

        // Collect secondary spans from duplicates
        for (size_t k = 1; k < indices.size(); ++k) {
            auto& dup = diagnostics_[indices[k]];
            if (dup.has_primary_span) {
                LabeledSpan ls;
                ls.span = dup.primary_span;
                ls.label = LazyMessage(std::string());
                first.secondary_spans.push_back(std::move(ls));
            }
            to_remove.push_back(indices[k]);
        }
    }

    std::sort(to_remove.begin(), to_remove.end());
    to_remove.erase(std::unique(to_remove.begin(), to_remove.end()), to_remove.end());
    for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
        auto& d = diagnostics_[*it];
        if (d.is_error() && error_count_ > 0) --error_count_;
        diagnostics_.erase(diagnostics_.begin() + static_cast<ptrdiff_t>(*it));
    }
}

void DiagnosticBag::sort_by_priority_and_position() {
    sorted_view_.clear();
    for (const auto& diag : diagnostics_) {
        sorted_view_.push_back(&diag);
    }

    std::sort(sorted_view_.begin(), sorted_view_.end(),
              [](const Diagnostic* a, const Diagnostic* b) {
                  int pa = a->priority_order();
                  int pb = b->priority_order();
                  if (pa != pb) return pa < pb;

                  // Same severity: sort by source position
                  if (a->has_primary_span && b->has_primary_span) {
                      if (a->primary_span.file_id != b->primary_span.file_id)
                          return a->primary_span.file_id < b->primary_span.file_id;
                      if (a->primary_span.start.line != b->primary_span.start.line)
                          return a->primary_span.start.line < b->primary_span.start.line;
                      return a->primary_span.start.index < b->primary_span.start.index;
                  }
                  // Diagnostics without spans go last
                  return a->has_primary_span > b->has_primary_span;
              });
}

void DiagnosticBag::cap_output() {
    if (sorted_view_.size() <= MAX_DIAGNOSTICS) return;

    // Keep only the first MAX_DIAGNOSTICS
    sorted_view_.resize(MAX_DIAGNOSTICS);
}

void DiagnosticBag::finalize() {
    if (finalized_) return;

    suppress_cascading();
    aggregate_duplicates();
    sort_by_priority_and_position();
    cap_output();

    finalized_ = true;
}

} // namespace zith::diag
