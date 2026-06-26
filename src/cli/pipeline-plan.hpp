#pragma once

#include <cstdint>

namespace zith::cli {

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
    MirLowered,
    ZirInterpreted
};

struct PipelinePlan {
    Stage current = Stage::Source;
    Stage target  = Stage::ZirInterpreted;

    bool shouldStop() const noexcept;
    void advance() noexcept;
};

} // namespace zith::cli
