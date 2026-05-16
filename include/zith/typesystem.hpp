// include/zith/typesystem.hpp — Type representation for semantic analysis
#pragma once

#include "zith/types.h"

namespace zith {

enum class SemaType {
    Unknown = 0,
    Void,
    Int,
    Float,
    String,
    Bool,
    Module,
};

struct Type {
    SemaType base = SemaType::Unknown;
    bool optional = false;
    bool failable = false;
    ZithOwnership ownership = ZITH_OWN_DEFAULT;
};

inline bool type_match(const Type &a, const Type &b) {
    return a.base == b.base && a.optional == b.optional && a.failable == b.failable;
}

} // namespace zith
