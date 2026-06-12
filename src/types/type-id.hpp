#pragma once

#include <cstdint>

namespace zith::types {

using TypeId = uint32_t;

inline constexpr TypeId kErrorType   = 0;
inline constexpr TypeId kNeverType   = 1;
inline constexpr TypeId kVoidType    = 2;
inline constexpr TypeId kBoolType    = 3;
inline constexpr TypeId kCharType    = 4;
inline constexpr TypeId kFirstCustom = 5;
inline constexpr TypeId kInvalidType = ~TypeId{0};

} // namespace zith::types
