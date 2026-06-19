#include "type-intern.hpp"

#include <cstring>

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

namespace {

bool typeDataEqual(const TypeData &a, const TypeData &b) {
    if (a.index() != b.index()) return false;
    switch (static_cast<TypeKind>(a.index())) {
    case TypeKind::Int:
        return std::get<TypeInt>(a).width == std::get<TypeInt>(b).width;
    case TypeKind::Float:
        return std::get<TypeFloat>(a).width == std::get<TypeFloat>(b).width;
    case TypeKind::Ptr:
        return std::get<TypePtr>(a).pointee == std::get<TypePtr>(b).pointee;
    case TypeKind::Array: {
        auto &aa = std::get<TypeArray>(a);
        auto &bb = std::get<TypeArray>(b);
        return aa.elem == bb.elem && aa.count == bb.count;
    }
    case TypeKind::Struct:
        return std::get<TypeStruct>(a).def_id == std::get<TypeStruct>(b).def_id;
    case TypeKind::Fn: {
        auto &fa = std::get<TypeFn>(a);
        auto &fb = std::get<TypeFn>(b);
        if (fa.param_count != fb.param_count) return false;
        if (fa.ret != fb.ret) return false;
        for (size_t i = 0; i < fa.param_count; i++)
            if (fa.params[i] != fb.params[i]) return false;
        return true;
    }
    case TypeKind::TypeVar:
        return std::get<TypeTypeVar>(a).id == std::get<TypeTypeVar>(b).id;
    case TypeKind::Optional:
        return std::get<TypeOptional>(a).inner == std::get<TypeOptional>(b).inner;
    case TypeKind::Failable:
        return std::get<TypeFailable>(a).inner == std::get<TypeFailable>(b).inner;
    default:
        return true; // Error, Never, Void, Bool, Char, Opaque — no fields
    }
}

} // anonymous namespace

TypeIntern::TypeIntern(memory::Arena &arena)
    : arena_(arena), types_(arena), struct_defs_(arena) {
    types_.push(TypeError{});
    types_.push(TypeNever{});
    types_.push(TypeVoid{});
    types_.push(TypeBool{});
    types_.push(TypeChar{});
}

TypeId TypeIntern::intern(TypeData data) {
    // Skip dedup for type variables — each call creates a fresh variable
    if (!std::holds_alternative<TypeTypeVar>(data)) {
        for (TypeId i = kFirstCustom; i < static_cast<TypeId>(types_.size()); i++) {
            if (typeDataEqual(types_[i], data))
                return i;
        }
    }
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
    auto *param_copy = static_cast<TypeId *>(
        arena_.alloc(params.size() * sizeof(TypeId), alignof(TypeId)));
    std::memcpy(param_copy, params.data(), params.size() * sizeof(TypeId));
    return intern(TypeFn{param_copy, params.size(), ret});
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

// ── Struct definition helpers ──────────────────────────────────────

TypeId TypeIntern::defineStruct(std::string_view name) {
    TypeId def_id = static_cast<TypeId>(struct_defs_.size());
    struct_defs_.push(StructDef{name, memory::DynArray<StructField>(arena_)});
    return intern(TypeStruct{def_id});
}

void TypeIntern::addField(TypeId struct_type, std::string_view field_name, TypeId field_type) {
    auto &def = getStructDef(struct_type);
    def.fields.push(StructField{field_name, field_type});
}

const StructDef &TypeIntern::getStructDef(TypeId struct_type) const {
    auto &td = std::get<TypeStruct>(types_[struct_type]);
    return struct_defs_[td.def_id];
}

StructDef &TypeIntern::getStructDef(TypeId struct_type) {
    return const_cast<StructDef &>(const_cast<const TypeIntern *>(this)->getStructDef(struct_type));
}

size_t TypeIntern::fieldCount(TypeId struct_type) const {
    return getStructDef(struct_type).fields.size();
}

const StructField &TypeIntern::getField(TypeId struct_type, size_t index) const {
    return getStructDef(struct_type).fields[index];
}

bool TypeIntern::hasField(TypeId struct_type, std::string_view name) const {
    auto &def = getStructDef(struct_type);
    for (auto &f : def.fields) {
        if (f.name == name)
            return true;
    }
    return false;
}

TypeId TypeIntern::fieldType(TypeId struct_type, std::string_view name) const {
    auto &def = getStructDef(struct_type);
    for (auto &f : def.fields) {
        if (f.name == name)
            return f.type;
    }
    return kErrorType;
}

// ── Query ─────────────────────────────────────────────────────────

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
