#pragma once

#include <cstdint>

namespace zith::import {

enum class SymbolVisibility : uint8_t {
    Private,
    Public,
    Module,
};

} // namespace zith::import
