#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "import-types.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/optional.hpp"
#include "memory/string-interner.hpp"

namespace zith::symbols {

class ImportRegistry {
public:
    explicit ImportRegistry(memory::Arena &arena);

    auto add(LoadedFile file) -> size_t;
    const LoadedFile &get(size_t idx) const;
    size_t fileCount() const noexcept;
    memory::Optional<SymOrigin> originOf(SymId main_sym) const;

    void mergeInto(memory::Arena &arena, memory::StringInterner &interner,
                   diagnostics::DiagnosticEngine &diags, SymbolTable &main_syms,
                   int32_t from_depth = 0);

private:
    memory::DynArray<LoadedFile> files_;
    memory::DynArray<SymOrigin> sym_origins_;
};

} // namespace zith::symbols
