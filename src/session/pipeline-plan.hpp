#pragma once

#include <cstdint>

namespace zith::session {

enum class Stage : uint8_t {
    Source,
    Lexed,
    Scanned,
    Imported,
    Resolved,
    TypeChecked,
    Solved,
    SraResolved,
    HirLowered,
    CodegenReady,
    Cached,

    /// Compatibility alias for the pre-rename spelling. The Zith-- slice is
    /// SRA, but external consumers (for example zith-lsp) still reference the
    /// historical NRA name. The numeric value is unchanged.
    NraResolved = SraResolved
};

struct PipelinePlan {
    Stage current = Stage::Source;
    Stage target  = Stage::Cached;

    bool shouldStop() const noexcept;
    void advance() noexcept;
};

} // namespace zith::session
