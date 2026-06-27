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
    NraResolved,
    HirLowered,
    CodegenReady,
    Cached
};

struct PipelinePlan {
    Stage current = Stage::Source;
    Stage target  = Stage::Cached;

    bool shouldStop() const noexcept;
    void advance() noexcept;
};

} // namespace zith::session
