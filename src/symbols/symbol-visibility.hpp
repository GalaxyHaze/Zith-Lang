#pragma once

#include <cstdint>

namespace zith::symbols {

enum class SymbolVisibility : uint8_t {
    Private,
    Public,
    Module,
};

} // namespace zith::symbols
