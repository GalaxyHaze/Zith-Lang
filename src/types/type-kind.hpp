#pragma once

#include "types/type-id.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace zith::types {

enum class IntWidth : uint8_t { I8, I16, I32, I64, I128, U8, U16, U32, U64, U128 };

enum class FloatWidth : uint8_t { F32, F64, F128 };

struct TypeInt {
    IntWidth width;
};
struct TypeFloat {
    FloatWidth width;
};
struct TypePtr {
    TypeId pointee;
};
struct TypeArray {
    TypeId elem;
    uint32_t count;
};
struct TypeStruct {
    memory::DynArray<TypeId> fields;
};
struct TypeFn {
    memory::DynArray<TypeId> params;
    TypeId ret;
};
struct TypeTypeVar {
    uint32_t id;
};

enum class TypeKind : uint8_t {
    Int,
    Float,
    Bool,
    Char,
    Void,
    Never,
    Ptr,
    Array,
    Struct,
    Fn,
    TypeVar,
    Error
};

using TypeData =
    std::variant<TypeInt, TypeFloat, TypePtr, TypeArray, TypeStruct, TypeFn, TypeTypeVar>;

} // namespace zith::types
