// include/zith/typesystem.hpp — Type representation for semantic analysis
#pragma once

#include "zith/types.h"
#include <cstring>

namespace zith {

enum class SemaType {
    Unknown = 0,
    Void,
    Char,
    Int,
    Float,
    String,
    Bool,
    Opaque,
    Invalid,
    VoidPtr,
    Module,
    Struct,
    Array,
    Slice,
};

struct Type {
    SemaType base = SemaType::Unknown;
    bool optional = false;
    bool failable = false;
    ZithOwnership ownership = ZITH_OWN_DEFAULT;
    const char *struct_name = nullptr;
    const char *type_name = nullptr;
    size_t array_size = 0;
    Type *element_type = nullptr;
};

inline bool type_match(const Type &a, const Type &b) {
    if (a.base != b.base || a.optional != b.optional || a.failable != b.failable ||
        a.ownership != b.ownership)
        return false;
    if (a.base == SemaType::Struct)
        return a.struct_name && b.struct_name && strcmp(a.struct_name, b.struct_name) == 0;
    if (a.base == SemaType::Array || a.base == SemaType::Slice) {
        if (a.base == SemaType::Array && a.array_size != b.array_size)
            return false;
        if (!a.element_type || !b.element_type)
            return a.element_type == b.element_type;
        return type_match(*a.element_type, *b.element_type);
    }
    return true;
}

} // namespace zith
