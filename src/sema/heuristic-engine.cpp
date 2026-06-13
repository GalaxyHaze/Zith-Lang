#include "sema/heuristic-engine.hpp"
#include "diagnostics/error-codes.hpp"

#include <algorithm>
#include <limits>

namespace zith::sema {

namespace err = diagnostics::err;

size_t HeuristicEngine::levenshteinDistance(std::string_view a, std::string_view b) {
    size_t n = a.size();
    size_t m = b.size();
    if (n == 0) return m;
    if (m == 0) return n;

    std::vector<size_t> prev(m + 1);
    std::vector<size_t> curr(m + 1);

    for (size_t j = 0; j <= m; ++j) prev[j] = j;
    for (size_t i = 1; i <= n; ++i) {
        curr[0] = i;
        for (size_t j = 1; j <= m; ++j) {
            size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, curr);
    }
    return prev[m];
}

std::string_view HeuristicEngine::findBestMatch(std::string_view target,
                                                  const import::SymbolTable &syms) {
    std::string_view best;
    size_t best_dist = std::numeric_limits<size_t>::max();

    for (size_t i = 0; i < syms.symbolCount(); ++i) {
        auto &data = syms.get(static_cast<import::SymId>(i));
        if (data.name.empty()) continue;
        size_t dist = levenshteinDistance(target, data.name);
        size_t max_dist = std::max(target.size(), data.name.size()) / 3 + 1;
        if (dist < best_dist && dist <= max_dist) {
            best_dist = dist;
            best = data.name;
        }
    }
    return best;
}

void HeuristicEngine::generate(const diagnostics::Diagnostic &diag, import::SymbolTable &syms,
                                std::vector<std::string> &out_suggestions) const {
    switch (diag.code) {
    case err::UndefinedIdent: {
        auto start = diag.message.find('\'');
        auto end = diag.message.rfind('\'');
        if (start != std::string::npos && end > start) {
            std::string_view unknown(diag.message.data() + start + 1, end - start - 1);
            std::string_view best = findBestMatch(unknown, syms);
            if (!best.empty() && best != unknown) {
                out_suggestions.push_back(
                    std::string("did you mean '") + std::string(best) + "'?");
            }
        }
        break;
    }

    case err::TypeMismatch: {
        out_suggestions.push_back("use an explicit cast with `as <type>` or convert via a method");
        break;
    }
    default:
        break;
    }
}

} // namespace zith::diagnostics
