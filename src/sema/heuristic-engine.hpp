#pragma once

#include "diagnostics/diagnostic.hpp"
#include "import/symbol-table.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zith::sema {

class HeuristicEngine {
public:
    void generate(const diagnostics::Diagnostic &diag, import::SymbolTable &syms,
                  std::vector<std::string> &out_suggestions) const;

private:
    static size_t levenshteinDistance(std::string_view a, std::string_view b);
    static std::string_view findBestMatch(std::string_view target,
                                          const import::SymbolTable &syms);
};

} // namespace zith::diagnostics
