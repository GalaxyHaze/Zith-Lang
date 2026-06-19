#pragma once

#include "type-kind.hpp"

namespace zith::types {

/// Calls `fn` for each direct child TypeId in the type data.
/// All TypeKinds are covered; leaf types (Error, Never, Void, etc.)
/// simply call with no children.
template <typename F> void walkSubTypes(const TypeData &data, F &&fn) {
    switch (static_cast<TypeKind>(data.index())) {
    case TypeKind::Error:
    case TypeKind::Never:
    case TypeKind::Void:
    case TypeKind::Bool:
    case TypeKind::Char:
    case TypeKind::Opaque:
        break;

    case TypeKind::Int:
    case TypeKind::Float:
    case TypeKind::TypeVar:
        break;

    case TypeKind::Ptr:
        fn(std::get<TypePtr>(data).pointee);
        break;

    case TypeKind::Array:
        fn(std::get<TypeArray>(data).elem);
        break;

    case TypeKind::Struct:
        fn(std::get<TypeStruct>(data).def_id);
        break;

    case TypeKind::Fn: {
        auto &fnData = std::get<TypeFn>(data);
        fn(fnData.ret);
        for (size_t i = 0; i < fnData.param_count; i++)
            fn(fnData.params[i]);
        break;
    }

    case TypeKind::Optional:
        fn(std::get<TypeOptional>(data).inner);
        break;

    case TypeKind::Failable:
        fn(std::get<TypeFailable>(data).inner);
        break;

    case TypeKind::Unknown:
    case TypeKind::Enum:
    case TypeKind::Union:
    case TypeKind::GenericParam:
        break;

    case TypeKind::Slice:
        fn(std::get<TypeSlice>(data).elem);
        break;

    case TypeKind::Pack: {
        auto &p = std::get<TypePack>(data);
        for (size_t i = 0; i < p.count; i++)
            fn(p.members[i]);
        break;
    }

    case TypeKind::Sum: {
        auto &s = std::get<TypeSum>(data);
        for (size_t i = 0; i < s.count; i++)
            fn(s.members[i]);
        break;
    }

    case TypeKind::Incomplete: {
        auto &ic = std::get<TypeIncomplete>(data);
        fn(ic.base);
        for (size_t i = 0; i < ic.arg_count; i++)
            fn(ic.args[i]);
        break;
    }
    }
}

} // namespace zith::types
