#pragma once

#include <cstdint>

namespace zith::types {

using TypeId = uint32_t;

inline constexpr TypeId kErrorType   = 0;
inline constexpr TypeId kVoidType    = 1;
inline constexpr TypeId kBoolType    = 2;
inline constexpr TypeId kCharType    = 3;
inline constexpr TypeId kIntType     = 4;
inline constexpr TypeId kFloatType   = 5;
inline constexpr TypeId kFirstCustom = 6;
inline constexpr TypeId kInvalidType = ~TypeId{0};

} // namespace zith::types
