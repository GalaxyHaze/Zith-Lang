#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/string-interner.hpp"
#include "types/type-kind.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace zith::types {

struct StructField {
    memory::InternedId name;
    TypeId type;
};

struct StructDef {
    memory::InternedId name;
    memory::DynArray<StructField> fields;
};

class TypeIntern {
    memory::Arena &arena_;
    memory::StringInterner &interner_;
    memory::DynArray<TypeData> types_;
    memory::DynArray<size_t> hashes_;
    memory::DynArray<StructDef> struct_defs_;
    std::unordered_map<std::string, TypeId> named_types_;
    uint32_t next_enum_def_id_  = 0;
    uint32_t next_union_def_id_ = 0;

    size_t computeHash(const TypeData &data);

public:
    explicit TypeIntern(memory::Arena &arena, memory::StringInterner &interner);

    // ── Intern existing TypeData ─────────────────────────────────
    TypeId intern(TypeData data);

    // ── Convenience intern helpers ───────────────────────────────
    TypeId internInt(IntWidth w);
    TypeId internFloat(FloatWidth w);
    TypeId internPtr(TypeId pointee, bool is_mut = false,
                     OwnershipKind ownership = OwnershipKind::Default);
    TypeId internArray(TypeId elem, uint32_t count);
    TypeId internFn(std::span<const TypeId> params, TypeId ret);
    TypeId internOptional(TypeId inner);
    TypeId internFailable(TypeId inner);
    TypeId internTypeVar();
    TypeId internUnknown();
    TypeId registerNamedType(std::string_view name, TypeKind kind);
    void registerTypeAlias(std::string_view name, TypeId target);
    TypeId lookupNamedType(std::string_view name) const;
    TypeId internSlice(TypeId elem);
    TypeId internEnum(TypeId def_id);
    TypeId internUnion(TypeId def_id);
    TypeId internPack(std::span<const TypeId> members, std::span<const std::string_view> names);
    TypeId internSum(std::span<const TypeId> members);
    TypeId internGenericParam(uint32_t decl_id, uint32_t param_index);
    TypeId internIncomplete(TypeId base, std::span<const TypeId> args);

    // ── Struct definition helpers ─────────────────────────────────
    TypeId defineStruct(std::string_view name);
    void addField(TypeId struct_type, std::string_view field_name, TypeId field_type);

    const StructDef &getStructDef(TypeId struct_type) const;
    StructDef &getStructDef(TypeId struct_type);
    size_t fieldCount(TypeId struct_type) const;
    const StructField &getField(TypeId struct_type, size_t index) const;
    bool hasField(TypeId struct_type, std::string_view name);
    TypeId fieldType(TypeId struct_type, std::string_view name);

    // ── Query ────────────────────────────────────────────────────
    const TypeData &lookup(TypeId id) const;
    TypeKind kindOf(TypeId id) const;
    size_t count() const noexcept;
};

} // namespace zith::types
