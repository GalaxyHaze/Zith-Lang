#include "diagnostics/heuristic_engine.hpp"
#include "diagnostics/format.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace zith::diag {

size_t HeuristicEngine::levenshtein_distance(std::string_view a, std::string_view b) {
    size_t n = a.size();
    size_t m = b.size();
    if (n == 0) return m;
    if (m == 0) return n;

    // Use single row optimization
    std::vector<size_t> prev(m + 1);
    std::vector<size_t> curr(m + 1);

    for (size_t j = 0; j <= m; ++j) prev[j] = j;

    for (size_t i = 1; i <= n; ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= m; ++j) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j] + 1,       // deletion
                curr[j - 1] + 1,   // insertion
                prev[j - 1] + cost // substitution
            });
        }
        std::swap(prev, curr);
    }
    return prev[m];
}

std::string_view HeuristicEngine::find_best_match(
    std::string_view target,
    const std::vector<std::string>& candidates) const {

    std::string_view best;
    size_t best_dist = SIZE_MAX;

    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        size_t dist = levenshtein_distance(target, candidate);
        // Only suggest if distance is small relative to length
        size_t max_dist = std::max(target.size(), candidate.size()) / 3 + 1;
        if (dist < best_dist && dist <= max_dist) {
            best_dist = dist;
            best = candidate;
        }
    }
    return best;
}

std::vector<Suggestion> HeuristicEngine::generate(const Diagnostic& diag) const {
    switch (diag.code) {
    case DiagCode::UndefinedIdentifier:
        return on_undefined_identifier(diag);
    case DiagCode::TypeMismatch:
        return on_type_mismatch(diag);
    case DiagCode::ImmutableAssignment:
        return on_immutable_assignment(diag);
    case DiagCode::MissingSemicolon:
        return on_missing_semicolon(diag);
    case DiagCode::UnmatchedDelimiter:
        return on_unmatched_delimiter(diag);
    default:
        return {};
    }
}

std::vector<Suggestion> HeuristicEngine::on_undefined_identifier(
    const Diagnostic& diag) const {

    std::vector<Suggestion> result;
    std::string msg = diag.message.get();

    if (known_names_.empty()) return result;

    // Try to extract the unknown identifier from the message
    // Messages are typically: "undefined identifier `foo`"
    auto backtick_start = msg.find('`');
    auto backtick_end = msg.rfind('`');
    if (backtick_start == std::string::npos || backtick_end == backtick_start)
        return result;

    std::string_view unknown(
        msg.data() + backtick_start + 1,
        backtick_end - backtick_start - 1);

    std::string_view best = find_best_match(unknown, known_names_);
    if (!best.empty() && best != unknown) {
        Suggestion sug;
        sug.span = diag.primary_span;
        sug.replacement = std::string(best);
        sug.label = diag_format("did you mean `{}`?", best);
        sug.is_machine_applicable = false;
        result.push_back(std::move(sug));
    }

    return result;
}

std::vector<Suggestion> HeuristicEngine::on_type_mismatch(
    const Diagnostic& /*diag*/) const {

    std::vector<Suggestion> result;

    // Type mismatch suggestions are context-dependent and best handled
    // by the type checker itself. Here we provide generic fallbacks.

    Suggestion sug;
    sug.label = "use an explicit cast with `as <type>` or convert with a method";
    sug.is_machine_applicable = false;
    result.push_back(std::move(sug));

    return result;
}

std::vector<Suggestion> HeuristicEngine::on_immutable_assignment(
    const Diagnostic& /*diag*/) const {

    std::vector<Suggestion> result;

    {
        Suggestion sug;
        sug.label = "use `var` instead of `let` to make the binding mutable";
        sug.is_machine_applicable = false;
        result.push_back(std::move(sug));
    }
    {
        Suggestion sug;
        sug.label = "or create a new `var` binding and assign to that instead";
        sug.is_machine_applicable = false;
        result.push_back(std::move(sug));
    }

    return result;
}

std::vector<Suggestion> HeuristicEngine::on_missing_semicolon(
    const Diagnostic& /*diag*/) const {

    std::vector<Suggestion> result;
    Suggestion sug;
    sug.label = "add a `;` at the end of this statement";
    sug.is_machine_applicable = true;
    result.push_back(std::move(sug));
    return result;
}

std::vector<Suggestion> HeuristicEngine::on_unmatched_delimiter(
    const Diagnostic& /*diag*/) const {

    std::vector<Suggestion> result;
    Suggestion sug;
    sug.label = "check that all braces, brackets, and parentheses are properly matched";
    sug.is_machine_applicable = false;
    result.push_back(std::move(sug));
    return result;
}

} // namespace zith::diag
