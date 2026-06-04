#pragma once

#include "types/type-id.hpp"

#include <cstdint>

namespace zith::zir::hir {

    using HirTypeId = types::TypeId;

    enum class HirTypeKind : uint8_t {
        Int,
        Float,
        Bool,
        Char,
        Void,
        Never,
        Ptr,
        Array,
        Struct,
        Fn
    };

} // namespace zith::zir::hir
