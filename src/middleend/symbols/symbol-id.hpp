#pragma once

#include <cstdint>

namespace zith::middleend::symbols {

    using SymId   = uint32_t;
    using ScopeId = uint32_t;

    inline constexpr SymId kInvalidSym     = ~SymId{0};
    inline constexpr ScopeId kRootScope    = 0;
    inline constexpr ScopeId kInvalidScope = ~ScopeId{0};

} // namespace zith::middleend::symbols
