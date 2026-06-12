#include "type-intern.hpp"

namespace zith::types {

namespace {

TypeKind kindFromIndex(size_t idx) {
    static constexpr TypeKind map[] = {
        TypeKind::Error,     // TypeError    → 0
        TypeKind::Never,     // TypeNever    → 1
        TypeKind::Void,      // TypeVoid     → 2
        TypeKind::Bool,      // TypeBool     → 3
        TypeKind::Char,      // TypeChar     → 4
        TypeKind::Int,       // TypeInt      → 5
        TypeKind::Float,     // TypeFloat    → 6
        TypeKind::Ptr,       // TypePtr      → 7
        TypeKind::Array,     // TypeArray    → 8
        TypeKind::Struct,    // TypeStruct   → 9
        TypeKind::Fn,        // TypeFn       → 10
        TypeKind::TypeVar,   // TypeTypeVar  → 11
        TypeKind::Optional,  // TypeOptional → 12
        TypeKind::Failable,  // TypeFailable → 13
        TypeKind::Opaque,    // TypeOpaque   → 14
    };
    return (idx < std::size(map)) ? map[idx] : TypeKind::Error;
}

} // anonymous namespace

TypeIntern::TypeIntern(memory::Arena &arena) : types_(arena) {
    types_.push(TypeError{});
    types_.push(TypeNever{});
    types_.push(TypeVoid{});
    types_.push(TypeBool{});
    types_.push(TypeChar{});
}

TypeId TypeIntern::intern(TypeData data) {
    TypeId id = static_cast<TypeId>(types_.size());
    types_.push(std::move(data));
    return id;
}

TypeId TypeIntern::internInt(IntWidth w) {
    return intern(TypeInt{w});
}

TypeId TypeIntern::internFloat(FloatWidth w) {
    return intern(TypeFloat{w});
}

TypeId TypeIntern::internPtr(TypeId pointee) {
    return intern(TypePtr{pointee});
}

TypeId TypeIntern::internArray(TypeId elem, uint32_t count) {
    return intern(TypeArray{elem, count});
}

TypeId TypeIntern::internFn(std::span<const TypeId> params, TypeId ret) {
    return intern(TypeFn{params, ret});
}

TypeId TypeIntern::internStruct(TypeId def_id) {
    return intern(TypeStruct{def_id});
}

TypeId TypeIntern::internOptional(TypeId inner) {
    return intern(TypeOptional{inner});
}

TypeId TypeIntern::internFailable(TypeId inner) {
    return intern(TypeFailable{inner});
}

TypeId TypeIntern::internTypeVar() {
    return intern(TypeTypeVar{static_cast<uint32_t>(types_.size())});
}

const TypeData &TypeIntern::lookup(TypeId id) const {
    return types_[id];
}

TypeKind TypeIntern::kindOf(TypeId id) const {
    if (id == kInvalidType || id >= types_.size())
        return TypeKind::Error;
    return kindFromIndex(types_[id].index());
}

size_t TypeIntern::count() const noexcept {
    return types_.size();
}

} // namespace zith::types
