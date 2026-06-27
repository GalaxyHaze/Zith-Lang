#pragma once

#include "diagnostics/diagnostic.hpp"
#include "symbols/symbol-table.hpp"
#include "memory/dyn-array.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zith::sema {

class HeuristicEngine {
public:
    void generate(const diagnostics::Diagnostic &diag, symbols::SymbolTable &syms,
                  memory::DynArray<std::string> &out_suggestions) const;

private:
    static size_t levenshteinDistance(std::string_view a, std::string_view b);
    static std::string_view findBestMatch(std::string_view target, const symbols::SymbolTable &syms);
};

} // namespace zith::sema
