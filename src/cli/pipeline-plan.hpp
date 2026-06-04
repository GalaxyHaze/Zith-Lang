#pragma once

#include <cstdint>

namespace zith::driver {

    enum class Stage : uint8_t {
        Source,
        Lexed,
        Parsed,
        Resolved,
        TypeChecked,
        HirLowered,
        MirLowered,
        Compiled
    };

    struct PipelinePlan {
        Stage current = Stage::Source;
        Stage target  = Stage::Compiled;

        bool shouldStop() const noexcept;
        void advance() noexcept;
    };

} // namespace zith::driver
