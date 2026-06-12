#pragma once

#include "types/type-id.hpp"

#include <cstdint>
#include <span>
#include <variant>

namespace zith::types {

enum class IntWidth : uint8_t { I8, I16, I32, I64, I128, U8, U16, U32, U64, U128 };
enum class FloatWidth : uint8_t { F32, F64, F128 };

struct TypeError     {};
struct TypeNever     {};
struct TypeVoid      {};
struct TypeBool      {};
struct TypeChar      {};

struct TypeInt       { IntWidth width; };
struct TypeFloat     { FloatWidth width; };
struct TypePtr       { TypeId pointee; };
struct TypeArray     { TypeId elem; uint32_t count; };
struct TypeStruct    { TypeId def_id; };
struct TypeFn        { std::span<const TypeId> params; TypeId ret; };
struct TypeTypeVar   { uint32_t id; };
struct TypeOptional  { TypeId inner; };
struct TypeFailable  { TypeId inner; };
struct TypeOpaque    {};

using TypeData = std::variant<TypeError, TypeNever, TypeVoid, TypeBool, TypeChar,
                              TypeInt, TypeFloat, TypePtr, TypeArray,
                              TypeStruct, TypeFn, TypeTypeVar,
                              TypeOptional, TypeFailable, TypeOpaque>;

enum class TypeKind : uint8_t {
    Error,
    Never,
    Void,
    Bool,
    Char,
    Int,
    Float,
    Ptr,
    Array,
    Struct,
    Fn,
    TypeVar,
    Optional,
    Failable,
    Opaque,
};

} // namespace zith::types
