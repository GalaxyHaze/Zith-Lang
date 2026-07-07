#pragma once

#include "memory/string-interner.hpp"
#include "types/type-id.hpp"

#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

namespace zith::types {

enum class IntWidth : uint8_t { I8, I16, I32, I64, I128, U8, U16, U32, U64, U128 };
enum class FloatWidth : uint8_t { F32, F64, F128 };

enum class OwnershipKind : uint8_t { Default, Unique, Share, Lend, View, Belong };

struct TypeError {};
struct TypeNever {};
struct TypeVoid {};
struct TypeBool {};
struct TypeChar {};

struct TypeInt {
    IntWidth width;
};
struct TypeFloat {
    FloatWidth width;
};
struct TypePtr {
    TypeId pointee;
    bool is_mut             = false;
    OwnershipKind ownership = OwnershipKind::Default;
};
struct TypeArray {
    TypeId elem;
    uint32_t count;
};
struct TypeStruct {
    TypeId def_id;
};
struct TypeFn {
    const TypeId *params;
    size_t param_count;
    TypeId ret;
};
struct TypeTypeVar {
    uint32_t id;
};
struct TypeOptional {
    TypeId inner;
};
struct TypeFailable {
    TypeId inner;
};
struct TypeOpaque {};
struct TypeUnknown {};
struct TypeSlice {
    TypeId elem;
};
struct TypeEnum {
    TypeId def_id;
};
struct TypeUnion {
    TypeId def_id;
};
struct TypePack {
    const TypeId *members;
    const memory::InternedId *names;
    size_t count;
};
struct TypeSum {
    const TypeId *members;
    size_t count;
};
struct TypeGenericParam {
    uint32_t decl_id;
    uint32_t param_index;
};
struct TypeIncomplete {
    TypeId base;
    const TypeId *args;
    size_t arg_count;
};

using TypeData =
    std::variant<TypeError, TypeNever, TypeVoid, TypeBool, TypeChar, TypeInt, TypeFloat, TypePtr,
                 TypeArray, TypeStruct, TypeFn, TypeTypeVar, TypeOptional, TypeFailable, TypeOpaque,
                 TypeUnknown, TypeSlice, TypeEnum, TypeUnion, TypePack, TypeSum, TypeGenericParam,
                 TypeIncomplete>;

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
    Unknown,
    Slice,
    Enum,
    Union,
    Pack,
    Sum,
    GenericParam,
    Incomplete,
};

} // namespace zith::types
