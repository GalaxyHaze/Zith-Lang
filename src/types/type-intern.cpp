#include "type-intern.hpp"

#include <cstring>

namespace zith::types {

namespace {

TypeKind kindFromIndex(size_t idx) {
    static constexpr TypeKind map[] = {
        TypeKind::Error,        // TypeError      → 0
        TypeKind::Never,        // TypeNever      → 1
        TypeKind::Void,         // TypeVoid       → 2
        TypeKind::Bool,         // TypeBool       → 3
        TypeKind::Char,         // TypeChar       → 4
        TypeKind::Int,          // TypeInt        → 5
        TypeKind::Float,        // TypeFloat      → 6
        TypeKind::Ptr,          // TypePtr        → 7
        TypeKind::Array,        // TypeArray      → 8
        TypeKind::Struct,       // TypeStruct     → 9
        TypeKind::Fn,           // TypeFn         → 10
        TypeKind::TypeVar,      // TypeTypeVar    → 11
        TypeKind::Optional,     // TypeOptional   → 12
        TypeKind::Failable,     // TypeFailable   → 13
        TypeKind::Opaque,       // TypeOpaque     → 14
        TypeKind::Unknown,      // TypeUnknown    → 15
        TypeKind::Slice,        // TypeSlice      → 16
        TypeKind::Enum,         // TypeEnum       → 17
        TypeKind::Union,        // TypeUnion      → 18
        TypeKind::Pack,         // TypePack       → 19
        TypeKind::Sum,          // TypeSum        → 20
        TypeKind::GenericParam, // TypeGenericParam → 21
        TypeKind::Incomplete,   // TypeIncomplete → 22
    };
    return (idx < std::size(map)) ? map[idx] : TypeKind::Error;
}

} // anonymous namespace

namespace {

bool typeDataEqual(const TypeData &a, const TypeData &b) {
    if (a.index() != b.index())
        return false;
    switch (static_cast<TypeKind>(a.index())) {
    case TypeKind::Int:
        return std::get<TypeInt>(a).width == std::get<TypeInt>(b).width;
    case TypeKind::Float:
        return std::get<TypeFloat>(a).width == std::get<TypeFloat>(b).width;
    case TypeKind::Ptr: {
        auto &pa = std::get<TypePtr>(a);
        auto &pb = std::get<TypePtr>(b);
        return pa.pointee == pb.pointee && pa.is_mut == pb.is_mut && pa.ownership == pb.ownership;
    }
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
        if (fa.param_count != fb.param_count)
            return false;
        if (fa.ret != fb.ret)
            return false;
        for (size_t i = 0; i < fa.param_count; i++)
            if (fa.params[i] != fb.params[i])
                return false;
        return true;
    }
    case TypeKind::TypeVar:
        return std::get<TypeTypeVar>(a).id == std::get<TypeTypeVar>(b).id;
    case TypeKind::Optional:
        return std::get<TypeOptional>(a).inner == std::get<TypeOptional>(b).inner;
    case TypeKind::Failable:
        return std::get<TypeFailable>(a).inner == std::get<TypeFailable>(b).inner;
    case TypeKind::Slice:
        return std::get<TypeSlice>(a).elem == std::get<TypeSlice>(b).elem;
    case TypeKind::Enum:
        return std::get<TypeEnum>(a).def_id == std::get<TypeEnum>(b).def_id;
    case TypeKind::Union:
        return std::get<TypeUnion>(a).def_id == std::get<TypeUnion>(b).def_id;
    case TypeKind::Pack: {
        auto &pa = std::get<TypePack>(a);
        auto &pb = std::get<TypePack>(b);
        if (pa.count != pb.count)
            return false;
        for (size_t i = 0; i < pa.count; i++) {
            if (pa.members[i] != pb.members[i])
                return false;
            if (pa.names[i] != pb.names[i])
                return false;
        }
        return true;
    }
    case TypeKind::Sum: {
        auto &sa = std::get<TypeSum>(a);
        auto &sb = std::get<TypeSum>(b);
        if (sa.count != sb.count)
            return false;
        for (size_t i = 0; i < sa.count; i++)
            if (sa.members[i] != sb.members[i])
                return false;
        return true;
    }
    case TypeKind::GenericParam: {
        auto &ga = std::get<TypeGenericParam>(a);
        auto &gb = std::get<TypeGenericParam>(b);
        return ga.decl_id == gb.decl_id && ga.param_index == gb.param_index;
    }
    case TypeKind::Incomplete: {
        auto &ia = std::get<TypeIncomplete>(a);
        auto &ib = std::get<TypeIncomplete>(b);
        if (ia.base != ib.base || ia.arg_count != ib.arg_count)
            return false;
        for (size_t i = 0; i < ia.arg_count; i++)
            if (ia.args[i] != ib.args[i])
                return false;
        return true;
    }
    default:
        return true; // Error, Never, Void, Bool, Char, Opaque, Unknown — no fields
    }
}

} // anonymous namespace

TypeIntern::TypeIntern(memory::Arena &arena, memory::StringInterner &interner)
    : arena_(arena), interner_(interner), types_(arena), hashes_(arena), struct_defs_(arena) {
    types_.push(TypeError{});
    hashes_.push(computeHash(types_.back()));
    types_.push(TypeNever{});
    hashes_.push(computeHash(types_.back()));
    types_.push(TypeVoid{});
    hashes_.push(computeHash(types_.back()));
    types_.push(TypeBool{});
    hashes_.push(computeHash(types_.back()));
    types_.push(TypeChar{});
    hashes_.push(computeHash(types_.back()));
}

namespace {

uint64_t hashCombine(uint64_t h, uint64_t v) {
    return (h ^ v) * 1099511628211ULL;
}

} // anonymous namespace

size_t TypeIntern::computeHash(const TypeData &data) {
    uint64_t h = 14695981039346656037ULL; // FNV offset basis
    h        = hashCombine(h, static_cast<size_t>(data.index()));

    switch (static_cast<TypeKind>(data.index())) {
    case TypeKind::Int:
        h = hashCombine(h, static_cast<size_t>(std::get<TypeInt>(data).width));
        break;
    case TypeKind::Float:
        h = hashCombine(h, static_cast<size_t>(std::get<TypeFloat>(data).width));
        break;
    case TypeKind::Ptr: {
        auto &p = std::get<TypePtr>(data);
        h       = hashCombine(h, p.pointee);
        h       = hashCombine(h, static_cast<size_t>(p.is_mut));
        h       = hashCombine(h, static_cast<size_t>(p.ownership));
        break;
    }
    case TypeKind::Array: {
        auto &a = std::get<TypeArray>(data);
        h       = hashCombine(h, a.elem);
        h       = hashCombine(h, a.count);
        break;
    }
    case TypeKind::Struct:
        h = hashCombine(h, std::get<TypeStruct>(data).def_id);
        break;
    case TypeKind::Fn: {
        auto &f = std::get<TypeFn>(data);
        h       = hashCombine(h, f.ret);
        h       = hashCombine(h, f.param_count);
        for (size_t i = 0; i < f.param_count; i++)
            h = hashCombine(h, f.params[i]);
        break;
    }
    case TypeKind::TypeVar:
        h = hashCombine(h, std::get<TypeTypeVar>(data).id);
        break;
    case TypeKind::Optional:
        h = hashCombine(h, std::get<TypeOptional>(data).inner);
        break;
    case TypeKind::Failable:
        h = hashCombine(h, std::get<TypeFailable>(data).inner);
        break;
    case TypeKind::Slice:
        h = hashCombine(h, std::get<TypeSlice>(data).elem);
        break;
    case TypeKind::Enum:
        h = hashCombine(h, std::get<TypeEnum>(data).def_id);
        break;
    case TypeKind::Union:
        h = hashCombine(h, std::get<TypeUnion>(data).def_id);
        break;
    case TypeKind::Pack: {
        auto &p = std::get<TypePack>(data);
        h       = hashCombine(h, p.count);
        for (size_t i = 0; i < p.count; i++) {
            h         = hashCombine(h, p.members[i]);
            auto name = interner_.lookup(p.names[i]);
            for (auto c : name)
                h = hashCombine(h, static_cast<size_t>(c));
        }
        break;
    }
    case TypeKind::Sum: {
        auto &s = std::get<TypeSum>(data);
        h       = hashCombine(h, s.count);
        for (size_t i = 0; i < s.count; i++)
            h = hashCombine(h, s.members[i]);
        break;
    }
    case TypeKind::GenericParam: {
        auto &g = std::get<TypeGenericParam>(data);
        h       = hashCombine(h, g.decl_id);
        h       = hashCombine(h, g.param_index);
        break;
    }
    case TypeKind::Incomplete: {
        auto &i = std::get<TypeIncomplete>(data);
        h       = hashCombine(h, i.base);
        h       = hashCombine(h, i.arg_count);
        for (size_t j = 0; j < i.arg_count; j++)
            h = hashCombine(h, i.args[j]);
        break;
    }
    default:
        break; // Error, Never, Void, Bool, Char, Opaque, Unknown — no fields
    }
    return static_cast<size_t>(h);
}

TypeId TypeIntern::intern(TypeData data) {
    // Skip dedup for type variables — each call creates a fresh variable
    if (!std::holds_alternative<TypeTypeVar>(data)) {
        size_t hash = computeHash(data);
        for (TypeId i = kFirstCustom; i < static_cast<TypeId>(types_.size()); i++) {
            if (hashes_[i] == hash && typeDataEqual(types_[i], data))
                return i;
        }
    }
    TypeId id = static_cast<TypeId>(types_.size());
    hashes_.push(computeHash(data));
    types_.push(std::move(data));
    return id;
}

TypeId TypeIntern::internInt(IntWidth w) {
    return intern(TypeInt{w});
}

TypeId TypeIntern::internFloat(FloatWidth w) {
    return intern(TypeFloat{w});
}

TypeId TypeIntern::internPtr(TypeId pointee, bool is_mut, OwnershipKind ownership) {
    return intern(TypePtr{pointee, is_mut, ownership});
}

TypeId TypeIntern::internArray(TypeId elem, uint32_t count) {
    return intern(TypeArray{elem, count});
}

TypeId TypeIntern::internFn(std::span<const TypeId> params, TypeId ret) {
    auto *param_copy =
        static_cast<TypeId *>(arena_.alloc(params.size() * sizeof(TypeId), alignof(TypeId)));
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

TypeId TypeIntern::internUnknown() {
    return intern(TypeUnknown{});
}

TypeId TypeIntern::registerNamedType(std::string_view name, TypeKind kind) {
    auto it = named_types_.find(std::string(name));
    if (it != named_types_.end())
        return it->second;

    TypeId tid = kErrorType;
    switch (kind) {
    case TypeKind::Struct:
        tid = defineStruct(name);
        break;
    case TypeKind::Enum:
        tid = internEnum(static_cast<TypeId>(next_enum_def_id_++));
        break;
    case TypeKind::Union:
        tid = internUnion(static_cast<TypeId>(next_union_def_id_++));
        break;
    default:
        return kErrorType;
    }
    named_types_[std::string(name)] = tid;
    return tid;
}

TypeId TypeIntern::lookupNamedType(std::string_view name) const {
    auto it = named_types_.find(std::string(name));
    if (it != named_types_.end())
        return it->second;
    return kErrorType;
}

void TypeIntern::registerTypeAlias(std::string_view name, TypeId target) {
    named_types_[std::string(name)] = target;
}

TypeId TypeIntern::internSlice(TypeId elem) {
    return intern(TypeSlice{elem});
}

TypeId TypeIntern::internEnum(TypeId def_id) {
    return intern(TypeEnum{def_id});
}

TypeId TypeIntern::internUnion(TypeId def_id) {
    return intern(TypeUnion{def_id});
}

TypeId TypeIntern::internPack(std::span<const TypeId> members,
                              std::span<const std::string_view> names) {
    size_t count = members.size();
    auto *m      = static_cast<TypeId *>(arena_.alloc(count * sizeof(TypeId), alignof(TypeId)));
    std::memcpy(m, members.data(), count * sizeof(TypeId));

    auto *n = static_cast<memory::InternedId *>(
        arena_.alloc(count * sizeof(memory::InternedId), alignof(memory::InternedId)));
    for (size_t i = 0; i < count; i++)
        n[i] = interner_.intern(names[i]);

    return intern(TypePack{m, n, count});
}

TypeId TypeIntern::internSum(std::span<const TypeId> members) {
    size_t count = members.size();
    auto *m      = static_cast<TypeId *>(arena_.alloc(count * sizeof(TypeId), alignof(TypeId)));
    std::memcpy(m, members.data(), count * sizeof(TypeId));
    return intern(TypeSum{m, count});
}

TypeId TypeIntern::internGenericParam(uint32_t decl_id, uint32_t param_index) {
    return intern(TypeGenericParam{decl_id, param_index});
}

TypeId TypeIntern::internIncomplete(TypeId base, std::span<const TypeId> args) {
    size_t count = args.size();
    auto *a      = static_cast<TypeId *>(arena_.alloc(count * sizeof(TypeId), alignof(TypeId)));
    std::memcpy(a, args.data(), count * sizeof(TypeId));
    return intern(TypeIncomplete{base, a, count});
}

// ── Struct definition helpers ──────────────────────────────────────

TypeId TypeIntern::defineStruct(std::string_view name) {
    TypeId def_id = static_cast<TypeId>(struct_defs_.size());
    struct_defs_.push(StructDef{interner_.intern(name), memory::DynArray<StructField>(arena_)});
    return intern(TypeStruct{def_id});
}

void TypeIntern::addField(TypeId struct_type, std::string_view field_name, TypeId field_type) {
    auto &def = getStructDef(struct_type);
    def.fields.push(StructField{interner_.intern(field_name), field_type});
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

bool TypeIntern::hasField(TypeId struct_type, std::string_view name) {
    auto &def = getStructDef(struct_type);
    auto id   = interner_.intern(name);
    for (auto &f : def.fields) {
        if (f.name == id)
            return true;
    }
    return false;
}

TypeId TypeIntern::fieldType(TypeId struct_type, std::string_view name) {
    auto &def = getStructDef(struct_type);
    auto id   = interner_.intern(name);
    for (auto &f : def.fields) {
        if (f.name == id)
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
