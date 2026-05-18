#pragma once

#include "diagnostics/diagnostic.hpp"

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

namespace zith::diag {

static constexpr size_t MAX_DIAGNOSTICS = 100;

class DiagnosticBag {
public:
    void emit(Diagnostic diag);

    void finalize();

    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    const std::vector<const Diagnostic*>& sorted_view() const { return sorted_view_; }

    bool had_errors() const { return error_count_ > 0; }
    bool had_fatal() const { return fatal_count_ > 0; }
    size_t error_count() const { return error_count_; }
    size_t warning_count() const { return warning_count_; }
    size_t total_count() const { return diagnostics_.size(); }

    bool is_finalized() const { return finalized_; }

    void clear();

private:
    std::vector<Diagnostic> diagnostics_;
    std::vector<const Diagnostic*> sorted_view_;
    size_t error_count_ = 0;
    size_t fatal_count_ = 0;
    size_t warning_count_ = 0;
    bool finalized_ = false;

    struct SpanIndex {
        FileId file_id;
        size_t start_line;
        size_t start_col;

        bool operator<(const SpanIndex& o) const {
            if (file_id != o.file_id) return file_id < o.file_id;
            if (start_line != o.start_line) return start_line < o.start_line;
            return start_col < o.start_col;
        }
    };

    SpanIndex get_span_key(const Diagnostic& d) const;

    void suppress_cascading();
    void aggregate_duplicates();
    void sort_by_priority_and_position();
    void cap_output();
};

} // namespace zith::diag
