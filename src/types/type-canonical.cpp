#include "types/type-canonical.hpp"

#include "common/overloaded.hpp"
#include "types/type-intern.hpp"
#include "types/type-kind.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace zith::types {

namespace {

constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime       = 1099511628211ULL;

uint32_t alignUp(uint32_t value, uint32_t align) noexcept {
    if (align == 0)
        return value;
    const uint32_t remainder = value % align;
    return remainder == 0 ? value : value + (align - remainder);
}

} // namespace

uint32_t tagByteCount(uint32_t member_count) noexcept {
    if (member_count <= 0xFFU)
        return 1U;
    if (member_count <= 0xFFFFU)
        return 2U;
    return 4U;
}

uint32_t typeByteCount(const TypeIntern &types, TypeId type) noexcept {
    switch (types.kindOf(type)) {
    case TypeKind::Bool:
    case TypeKind::Char:
        return 1;
    case TypeKind::Int: {
        const auto *integer = std::get_if<TypeInt>(&types.lookup(type));
        return integer != nullptr ? (intWidthBits(integer->width) + 7U) / 8U : 0U;
    }
    case TypeKind::Float: {
        const auto *floating = std::get_if<TypeFloat>(&types.lookup(type));
        if (floating == nullptr)
            return 0U;
        switch (floating->width) {
        case FloatWidth::F32:
            return 4U;
        case FloatWidth::F64:
            return 8U;
        case FloatWidth::F128:
            return 16U;
        }
        return 0U;
    }
    case TypeKind::Ptr:
        return 8U;
    case TypeKind::Optional: {
        const auto *optional = std::get_if<TypeOptional>(&types.lookup(type));
        if (optional == nullptr)
            return 0U;
        if (types.kindOf(optional->inner) == TypeKind::Ptr)
            return 8U;
        const auto inner_size  = typeByteCount(types, optional->inner);
        const auto inner_align = typeAlignBytes(types, optional->inner);
        return inner_size == 0U ? 0U : alignUp(alignUp(inner_size, 1U) + 1U, inner_align);
    }
    case TypeKind::Failable:
        return 8U;
    case TypeKind::Array: {
        const auto *array = std::get_if<TypeArray>(&types.lookup(type));
        return array != nullptr ? typeByteCount(types, array->elem) * array->count : 0U;
    }
    case TypeKind::Slice:
        return 16U;
    case TypeKind::Enum: {
        const auto *enumeration = std::get_if<TypeEnum>(&types.lookup(type));
        return enumeration != nullptr
                   ? typeByteCount(types, types.getEnumDef(enumeration->def_id).underlying)
                   : 0U;
    }
    case TypeKind::Union: {
        const auto *union_type = std::get_if<TypeUnion>(&types.lookup(type));
        if (union_type == nullptr)
            return 0U;
        const auto *def = types.lookupUnionDef(union_type->def_id);
        if (def == nullptr)
            return 0U;
        uint32_t max_bytes = 1U;
        uint32_t max_align = 1U;
        for (const auto member : def->members) {
            max_align = std::max(max_align, typeAlignBytes(types, member));
            max_bytes = std::max(max_bytes, typeByteCount(types, member));
        }
        if (!def->is_tagged)
            return alignUp(max_bytes, max_align);
        // Tagged unions append the smallest sufficient member-index tag after
        // the aligned payload.
        const auto payload_bytes = alignUp(max_bytes, max_align);
        return alignUp(payload_bytes + tagByteCount(static_cast<uint32_t>(def->members.size())),
                       max_align);
    }
    case TypeKind::Struct: {
        const auto *structure = std::get_if<TypeStruct>(&types.lookup(type));
        if (structure == nullptr)
            return 0U;
        const auto &def = types.getStructDef(structure->def_id);
        uint32_t offset = 0U;
        for (const auto &field : def.fields) {
            const auto align = typeAlignBytes(types, field.type);
            if (align == 0U)
                continue;
            offset = alignUp(offset, align);
            offset += typeByteCount(types, field.type);
        }
        return offset;
    }
    case TypeKind::Qualified: {
        const auto *qualified = std::get_if<TypeQualified>(&types.lookup(type));
        return qualified != nullptr ? typeByteCount(types, qualified->inner) : 0U;
    }
    case TypeKind::Alias: {
        const auto *alias = std::get_if<TypeAlias>(&types.lookup(type));
        return alias != nullptr ? typeByteCount(types, alias->target) : 0U;
    }
    case TypeKind::Nominal: {
        const auto *nominal = std::get_if<TypeNominal>(&types.lookup(type));
        return nominal != nullptr ? typeByteCount(types, nominal->target) : 0U;
    }
    default:
        return 0U;
    }
}

uint32_t typeAlignBytes(const TypeIntern &types, TypeId type) noexcept {
    switch (types.kindOf(type)) {
    case TypeKind::Bool:
    case TypeKind::Char:
        return 1U;
    case TypeKind::Int: {
        const auto *integer = std::get_if<TypeInt>(&types.lookup(type));
        return integer != nullptr ? ((intWidthBits(integer->width) + 7U) / 8U) : 0U;
    }
    case TypeKind::Float: {
        const auto *floating = std::get_if<TypeFloat>(&types.lookup(type));
        if (floating == nullptr)
            return 0U;
        switch (floating->width) {
        case FloatWidth::F32:
            return 4U;
        case FloatWidth::F64:
            return 8U;
        case FloatWidth::F128:
            return 16U;
        }
        return 0U;
    }
    case TypeKind::Ptr:
    case TypeKind::Failable:
        return 8U;
    case TypeKind::Optional: {
        const auto *optional = std::get_if<TypeOptional>(&types.lookup(type));
        if (optional == nullptr)
            return 0U;
        if (types.kindOf(optional->inner) == TypeKind::Ptr)
            return 8U;
        return typeAlignBytes(types, optional->inner);
    }
    case TypeKind::Array: {
        const auto *array = std::get_if<TypeArray>(&types.lookup(type));
        return array != nullptr ? typeAlignBytes(types, array->elem) : 0U;
    }
    case TypeKind::Slice:
        return 8U;
    case TypeKind::Enum: {
        const auto *enumeration = std::get_if<TypeEnum>(&types.lookup(type));
        return enumeration != nullptr
                   ? typeAlignBytes(types, types.getEnumDef(enumeration->def_id).underlying)
                   : 0U;
    }
    case TypeKind::Union: {
        const auto *union_type = std::get_if<TypeUnion>(&types.lookup(type));
        if (union_type == nullptr)
            return 0U;
        const auto *def = types.lookupUnionDef(union_type->def_id);
        if (def == nullptr)
            return 0U;
        uint32_t max_align = 1U;
        for (const auto member : def->members)
            max_align = std::max(max_align, typeAlignBytes(types, member));
        return max_align;
    }
    case TypeKind::Struct: {
        const auto *structure = std::get_if<TypeStruct>(&types.lookup(type));
        if (structure == nullptr)
            return 0U;
        uint32_t max_align = 1U;
        for (const auto &field : types.getStructDef(structure->def_id).fields)
            max_align = std::max(max_align, typeAlignBytes(types, field.type));
        return max_align;
    }
    case TypeKind::Qualified: {
        const auto *qualified = std::get_if<TypeQualified>(&types.lookup(type));
        return qualified != nullptr ? typeAlignBytes(types, qualified->inner) : 0U;
    }
    case TypeKind::Alias: {
        const auto *alias = std::get_if<TypeAlias>(&types.lookup(type));
        return alias != nullptr ? typeAlignBytes(types, alias->target) : 0U;
    }
    case TypeKind::Nominal: {
        const auto *nominal = std::get_if<TypeNominal>(&types.lookup(type));
        return nominal != nullptr ? typeAlignBytes(types, nominal->target) : 0U;
    }
    default:
        return 0U;
    }
}

std::vector<size_t> canonicalFieldOrder(const TypeIntern &types, TypeId type) {
    if (types.kindOf(type) != TypeKind::Struct)
        return {};
    // `getStructDef` takes the struct TypeId and resolves its def row itself.
    const auto &def  = types.getStructDef(type);
    const auto &pool = types.interner();
    std::vector<size_t> order(static_cast<size_t>(def.fields.size()));
    for (size_t index = 0; index < order.size(); ++index)
        order[index] = index;
    // Canonical field order is size-stable for opaque hydration: the same
    // concrete struct spelled in two modules must hash in identical field order.
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const size_t bytes_a = typeByteCount(types, def.fields[a].type);
        const size_t bytes_b = typeByteCount(types, def.fields[b].type);
        if (bytes_a != bytes_b)
            return bytes_a < bytes_b;
        return pool.lookup(def.fields[a].name) < pool.lookup(def.fields[b].name);
    });
    return order;
}

TypeCanonicalId canonicalTypeId(const TypeIntern &types, TypeId type,
                                const ModuleNamespaceResolver &resolveNamespace) {
    const auto &interner = types.interner();
    uint64_t hi_hash     = kFnvOffsetBasis;
    uint64_t lo_hash     = kFnvOffsetBasis;
    const auto append    = [&](const uint8_t *bytes, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            hi_hash ^= bytes[i];
            hi_hash *= kFnvPrime;
            lo_hash ^= static_cast<uint8_t>(bytes[i] ^ 0x5cu);
            lo_hash *= kFnvPrime;
        }
    };
    auto appendU64 = [&](uint64_t value) {
        const uint8_t raw[sizeof(value)] = {
            static_cast<uint8_t>(value),        static_cast<uint8_t>(value >> 8U),
            static_cast<uint8_t>(value >> 16U), static_cast<uint8_t>(value >> 24U),
            static_cast<uint8_t>(value >> 32U), static_cast<uint8_t>(value >> 40U),
            static_cast<uint8_t>(value >> 48U), static_cast<uint8_t>(value >> 56U)};
        append(raw, sizeof(raw));
    };
    const auto appendName = [&](memory::InternedId name) {
        const auto text = interner.lookup(name);
        append(reinterpret_cast<const uint8_t *>(text.data()), text.size());
    };

    appendU64(static_cast<uint64_t>(static_cast<TypeKind>(types.kindOf(type))));

    auto appendType = [&](const auto &self, TypeId current) -> void {
        const auto appendDefiningModule = [&]() {
            const auto namespace_text = resolveNamespace(types.definingModuleOf(current));
            append(reinterpret_cast<const uint8_t *>(namespace_text.data()), namespace_text.size());
        };
        const auto &data = types.lookup(current);
        std::visit(common::overloaded{
                       [&](const TypeError &) { appendU64(1); },
                       [&](const TypeNever &) { appendU64(2); },
                       [&](const TypeVoid &) { appendU64(3); },
                       [&](const TypeBool &) { appendU64(4); },
                       [&](const TypeChar &) { appendU64(5); },
                       [&](const TypeInt &t) {
                           appendU64(6);
                           appendU64(static_cast<uint64_t>(t.width));
                       },
                       [&](const TypeFloat &t) {
                           appendU64(7);
                           appendU64(static_cast<uint64_t>(t.width));
                       },
                       [&](const TypePtr &t) {
                           appendU64(8);
                           appendU64(static_cast<uint64_t>(t.is_mut));
                           appendU64(static_cast<uint64_t>(t.ownership));
                           self(self, t.pointee);
                       },
                       [&](const TypeArray &t) {
                           appendU64(9);
                           appendU64(t.count);
                           self(self, t.elem);
                       },
                       [&](const TypeStruct &) {
                           appendU64(10);
                           appendDefiningModule();
                           const auto &def = types.getStructDef(current);
                           appendName(def.name);
                           for (const size_t index : canonicalFieldOrder(types, current)) {
                               appendName(def.fields[index].name);
                               self(self, def.fields[index].type);
                           }
                       },
                       [&](const TypeFn &t) {
                           appendU64(11);
                           appendU64(t.param_count);
                           self(self, t.ret);
                           for (size_t i = 0; i < t.param_count; ++i)
                               self(self, t.params[i]);
                       },
                       [&](const TypeTypeVar &t) {
                           appendU64(12);
                           appendU64(t.id);
                       },
                       [&](const TypeOptional &t) {
                           appendU64(13);
                           self(self, t.inner);
                       },
                       [&](const TypeFailable &t) {
                           appendU64(14);
                           self(self, t.inner);
                       },
                       [&](const TypeAlias &t) {
                           appendU64(15);
                           self(self, t.target);
                       },
                       [&](const TypeNominal &t) {
                           appendU64(16);
                           appendName(t.name);
                           self(self, t.target);
                       },
                       [&](const TypeTrait &t) {
                           appendU64(17);
                           appendName(t.name);
                       },
                       [&](const TypeDyn &t) {
                           appendU64(18);
                           appendU64(t.method_count);
                           self(self, t.target);
                       },
                       [&](const TypeOpaque &) { appendU64(19); },
                       [&](const TypeOpaqueTagged &) { appendU64(20); },
                       [&](const TypeUnknown &) { appendU64(21); },
                       [&](const TypeQualified &t) {
                           appendU64(22);
                           appendU64(static_cast<uint64_t>(t.ownership));
                           appendU64(static_cast<uint64_t>(t.isMut));
                           self(self, t.inner);
                       },
                       [&](const TypeSlice &t) {
                           appendU64(23);
                           self(self, t.elem);
                       },
                       [&](const TypeEnum &) {
                           appendU64(24);
                           appendDefiningModule();
                           const auto &def = types.getEnumDef(current);
                           appendName(def.name);
                           self(self, def.underlying);
                           for (const auto &variant : def.variants) {
                               appendName(variant.name);
                               appendU64(static_cast<uint64_t>(variant.discriminant));
                           }
                       },
                       [&](const TypeUnion &) {
                           appendU64(25);
                           appendDefiningModule();
                           const auto &def = types.getUnionDef(current);
                           appendU64(static_cast<uint64_t>(def.is_tagged));
                           appendName(def.name);
                           for (const auto member : def.members)
                               self(self, member);
                       },
                       [&](const TypePack &t) {
                           appendU64(26);
                           appendU64(t.count);
                           for (size_t i = 0; i < t.count; ++i)
                               self(self, t.members[i]);
                       },
                       [&](const TypeSum &t) {
                           appendU64(27);
                           appendU64(t.count);
                           for (size_t i = 0; i < t.count; ++i)
                               self(self, t.members[i]);
                       },
                       [&](const TypeGenericParam &t) {
                           appendU64(28);
                           appendU64(t.decl_id);
                           appendU64(t.param_index);
                       },
                       [&](const TypeIncomplete &t) {
                           appendU64(29);
                           appendU64(t.arg_count);
                           self(self, t.base);
                           for (size_t i = 0; i < t.arg_count; ++i)
                               self(self, t.args[i]);
                       },
                   },
                   data);
    };
    appendType(appendType, type);

    TypeCanonicalId result;
    result.hi = hi_hash;
    result.lo = lo_hash;
    return result;
}

} // namespace zith::types
