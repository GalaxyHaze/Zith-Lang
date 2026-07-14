#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "import-types.hpp"
#include "symbols/symbol-table.hpp"

namespace zith::symbols {

class SymbolMerger {
public:
    SymbolMerger(memory::Arena &arena, memory::StringInterner &interner,
                 diagnostics::DiagnosticEngine &diags, const memory::DynArray<LoadedFile> &files,
                 memory::DynArray<SymOrigin> &sym_origins);

    void mergeInto(SymbolTable &main_syms, int32_t from_depth = 0);

private:
    memory::Arena &arena_;
    memory::StringInterner &interner_;
    diagnostics::DiagnosticEngine &diags_;
    const memory::DynArray<LoadedFile> &files_;
    memory::DynArray<SymOrigin> &sym_origins_;
};

} // namespace zith::symbols
