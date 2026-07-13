#include "import-registry.hpp"

#include "symbol-merger.hpp"

namespace zith::symbols {

ImportRegistry::ImportRegistry(memory::Arena &arena) : files_(arena), sym_origins_(arena) {}

auto ImportRegistry::add(LoadedFile file) -> size_t {
    auto idx = files_.size();
    files_.push(std::move(file));
    return idx;
}

const LoadedFile &ImportRegistry::get(size_t idx) const {
    return files_[idx];
}

size_t ImportRegistry::fileCount() const noexcept {
    return files_.size();
}

memory::Optional<SymOrigin> ImportRegistry::originOf(SymId main_sym) const {
    if (main_sym >= sym_origins_.size())
        return {};
    auto &origin = sym_origins_[main_sym];
    if (origin.file_idx == size_t(-1))
        return {};
    return origin;
}

void ImportRegistry::mergeInto(memory::Arena &arena, memory::StringInterner &interner,
                               diagnostics::DiagnosticEngine &diags, SymbolTable &main_syms,
                               int32_t from_depth) {
    SymbolMerger merger(arena, interner, diags, files_, sym_origins_);
    merger.mergeInto(main_syms, from_depth);
}

} // namespace zith::symbols
