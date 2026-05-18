#pragma once

#include "diagnostics/diagnostic.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zith::diag {

class HeuristicEngine {
public:
    std::vector<Suggestion> generate(const Diagnostic& diag) const;

    void set_known_names(std::vector<std::string> names) {
        known_names_ = std::move(names);
    }

    void add_known_name(std::string name) {
        known_names_.push_back(std::move(name));
    }

private:
    std::vector<std::string> known_names_;

    std::vector<Suggestion> on_undefined_identifier(const Diagnostic& diag) const;
    std::vector<Suggestion> on_type_mismatch(const Diagnostic& diag) const;
    std::vector<Suggestion> on_immutable_assignment(const Diagnostic& diag) const;
    std::vector<Suggestion> on_missing_semicolon(const Diagnostic& diag) const;
    std::vector<Suggestion> on_unmatched_delimiter(const Diagnostic& diag) const;

    static size_t levenshtein_distance(std::string_view a, std::string_view b);
    std::string_view find_best_match(std::string_view target, const std::vector<std::string>& candidates) const;
};

} // namespace zith::diag
