#include "zirl/zirl-reader.hpp"
#include "cache/cache-entry.hpp"
#include "zirl/zirl-attrs-section.hpp"
#include "zirl/zirl-buffer.hpp"
#include "zirl/zirl-code-section.hpp"
#include "zirl/zirl-decl-section.hpp"
#include "zirl/zirl-debug-section.hpp"
#include "zirl/zirl-header.hpp"
#include "zirl/zirl-instantiation-section.hpp"
#include "zirl/zirl-type-section.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <vector>

namespace zith::zirl {

namespace {

struct SectionView {
    SectionEntry entry;
    std::string_view payload;
};

constexpr std::array<SectionId, 5> kRequiredSections{
    SectionId::Metadata, SectionId::Decls, SectionId::Templates, SectionId::Code, SectionId::Attrs};

uint32_t checksumDependenciesAndSections(const cache::Artifact &artifact,
                                         const std::vector<SectionView> &sections) {
    uint32_t checksum = fnv1a32(artifact.canonical_path) ^ fnv1a32(artifact.module_name) ^
                        fnv1a32(artifact.semantic_profile.name);
    for (const auto &dep : artifact.deps) {
        checksum ^= fnv1a32(dep.canonical_path);
        checksum ^= fnv1a32(dep.import_key);
        uint32_t abi[2] = {dep.public_abi_hi, dep.public_abi_lo};
        checksum ^= fnv1a32(reinterpret_cast<const uint8_t *>(abi), sizeof(abi));
    }
    for (const auto &section : sections)
        checksum ^= fnv1a32(reinterpret_cast<const uint8_t *>(section.payload.data()),
                            section.payload.size());
    return checksum;
}

const SectionView *findSection(const std::vector<SectionView> &sections, SectionId id) {
    const auto raw_id = static_cast<uint16_t>(id);
    const auto it =
        std::find_if(sections.begin(), sections.end(),
                     [raw_id](const SectionView &section) { return section.entry.id == raw_id; });
    return it == sections.end() ? nullptr : &*it;
}

bool validateArtifactReferences(const cache::Artifact &artifact) {
    const auto validStringId = [&](uint32_t id) { return id < artifact.strings.size(); };

    for (const auto &type : artifact.types) {
        if (static_cast<uint8_t>(type.kind) >
            static_cast<uint8_t>(cache::CompactTypeKind::Trait))
            return false;
        if (type.kind == cache::CompactTypeKind::Trait && !validStringId(type.ref0))
            return false;
        if (type.kind == cache::CompactTypeKind::Pack) {
            if (!type.arg_names.empty() && type.arg_names.size() != type.args.size())
                return false;
            for (const auto name_id : type.arg_names)
                if (!validStringId(name_id))
                    return false;
        }
    }

    for (const auto &definition : artifact.struct_defs) {
        if (!validStringId(definition.name_id) ||
            definition.field_name_ids.size() != definition.field_type_ids.size() ||
            (!definition.foreign_field_offsets.empty() &&
             definition.foreign_field_offsets.size() != definition.field_type_ids.size()))
            return false;
        for (const auto name_id : definition.field_name_ids)
            if (!validStringId(name_id))
                return false;
        for (const auto type_id : definition.type_arg_ids)
            if (type_id >= artifact.types.size())
                return false;
    }

    for (const auto &definition : artifact.enum_defs) {
        if (!validStringId(definition.name_id))
            return false;
        for (const auto &variant : definition.variants)
            if (!validStringId(variant.name_id))
                return false;
    }

    for (const auto &definition : artifact.union_defs)
        if (!validStringId(definition.name_id))
            return false;

    for (const auto &expr : artifact.exprs) {
        const bool needs_name =
            (expr.kind == cache::CompactExprKind::Literal && expr.flags == 3U) ||
            expr.kind == cache::CompactExprKind::Let || expr.kind == cache::CompactExprKind::Var ||
            expr.kind == cache::CompactExprKind::GlobalConstLoad ||
            expr.kind == cache::CompactExprKind::ExternalGlobalAddr ||
            expr.kind == cache::CompactExprKind::MakeDyn ||
            expr.kind == cache::CompactExprKind::DynCall;
        if (needs_name && !validStringId(expr.name_id))
            return false;
    }

    std::vector<uint32_t> valid_slots;
    size_t slot_capacity = 0;
    for (size_t index = 0; index < artifact.functions.size(); ++index) {
        const auto &function = artifact.functions[index];
        if (!validStringId(function.name_id) ||
            (function.foreign_adapter_name_id != ~uint32_t{0} &&
             !validStringId(function.foreign_adapter_name_id)) ||
            function.param_type_ids.size() != function.param_name_ids.size() ||
            (!function.param_slot_ids.empty() &&
             function.param_slot_ids.size() != function.param_type_ids.size()))
            return false;
        for (const auto name_id : function.param_name_ids)
            if (!validStringId(name_id))
                return false;
        for (const auto slot_id : function.param_slot_ids) {
            if (slot_id != hir::kInvalidHirSlot)
                ++slot_capacity;
        }

        const auto resolved =
            cache::resolveFunctionReference(artifact, function.name_id, function.signature_type_id);
        if (!resolved || *resolved != index)
            return false;
    }

    for (const auto &expr : artifact.exprs)
        if (expr.kind == cache::CompactExprKind::SlotAlloca)
            ++slot_capacity;

    const auto appendSlot = [&](uint32_t slot_id) {
        if (slot_id == hir::kInvalidHirSlot || slot_id >= slot_capacity)
            return false;
        valid_slots.push_back(slot_id);
        return true;
    };
    for (const auto &function : artifact.functions)
        for (const auto slot_id : function.param_slot_ids)
            if (slot_id != hir::kInvalidHirSlot && !appendSlot(slot_id))
                return false;

    for (const auto &expr : artifact.exprs) {
        switch (expr.kind) {
        case cache::CompactExprKind::SlotAlloca:
            if (!appendSlot(expr.ref_a))
                return false;
            break;
        case cache::CompactExprKind::SlotStore:
        case cache::CompactExprKind::SlotLoad:
        case cache::CompactExprKind::SlotAddr:
            if (!appendSlot(expr.ref_a))
                return false;
            break;
        case cache::CompactExprKind::Pipe:
        case cache::CompactExprKind::PipeDo:
            if (!appendSlot(expr.ref_b))
                return false;
            break;
        case cache::CompactExprKind::PipeCurrent:
            if (!appendSlot(expr.ref_a))
                return false;
            break;
        default:
            break;
        }
    }
    std::sort(valid_slots.begin(), valid_slots.end());
    valid_slots.erase(std::unique(valid_slots.begin(), valid_slots.end()), valid_slots.end());

    for (const auto &expr : artifact.exprs) {
        const bool direct_call = expr.kind == cache::CompactExprKind::Call ||
                                 expr.kind == cache::CompactExprKind::StateTailCall;
        if (expr.resolved_function_name_id == ~uint32_t{0} &&
            expr.resolved_function_signature_type_id == ~uint32_t{0})
            continue;
        if (!direct_call ||
            !cache::resolveFunctionReference(artifact, expr.resolved_function_name_id,
                                             expr.resolved_function_signature_type_id))
            return false;
    }

    std::vector<bool> seen_slots(valid_slots.size(), false);
    std::vector<bool> seen_call_attrs(artifact.exprs.size(), false);
    std::vector<bool> seen_fn_attrs(artifact.functions.size(), false);
    for (const auto &slot : artifact.attrs_slots) {
        const auto found = std::lower_bound(valid_slots.begin(), valid_slots.end(), slot.slot);
        if (found == valid_slots.end() || *found != slot.slot)
            return false;
        const size_t index = static_cast<size_t>(found - valid_slots.begin());
        if (seen_slots[index])
            return false;
        seen_slots[index] = true;
    }
    for (const auto &call : artifact.attrs_calls) {
        if (call.expr_id >= artifact.exprs.size() || seen_call_attrs[call.expr_id])
            return false;
        const auto kind = artifact.exprs[call.expr_id].kind;
        if (kind != cache::CompactExprKind::Call && kind != cache::CompactExprKind::StateTailCall &&
            kind != cache::CompactExprKind::DynCall)
            return false;
        seen_call_attrs[call.expr_id] = true;
    }
    for (const auto &attrs : artifact.attrs_fns) {
        if (attrs.fn_index >= artifact.functions.size() || seen_fn_attrs[attrs.fn_index])
            return false;
        seen_fn_attrs[attrs.fn_index] = true;
    }

    for (const auto &vtable : artifact.vtables) {
        if (!validStringId(vtable.name_id) ||
            vtable.slot_function_name_ids.size() != vtable.slot_function_signature_type_ids.size())
            return false;
        for (size_t slot = 0; slot < vtable.slot_function_name_ids.size(); ++slot) {
            const auto name_id      = vtable.slot_function_name_ids[slot];
            const auto signature_id = vtable.slot_function_signature_type_ids[slot];
            if (name_id == ~uint32_t{0} && signature_id == ~uint32_t{0})
                continue;
            if (!cache::resolveFunctionReference(artifact, name_id, signature_id))
                return false;
        }
    }
    for (const auto &global : artifact.globals)
        if (!validStringId(global.name_id))
            return false;

    for (size_t index = 0; index < artifact.decls.size(); ++index) {
        const auto &decl = artifact.decls[index];
        if (!validStringId(decl.name_id) ||
            (decl.type_id != ~uint32_t{0} && decl.type_id >= artifact.types.size()) ||
            (decl.template_index != ~uint32_t{0} &&
             decl.template_index >= artifact.templates.size()) ||
            (decl.body_fn_index != ~uint32_t{0} &&
             decl.body_fn_index >= artifact.functions.size()) ||
            ((decl.kind == cache::CompactSymKind::Struct ||
              decl.kind == cache::CompactSymKind::Component) &&
             decl.field_name_ids.size() != decl.field_type_ids.size()))
            return false;
        if (decl.template_index != ~uint32_t{0} &&
            artifact.templates[decl.template_index].declaration_index != index)
            return false;
        for (const auto name_id : decl.field_name_ids)
            if (!validStringId(name_id))
                return false;
        for (const auto type_id : decl.field_type_ids)
            if (type_id >= artifact.types.size())
                return false;
        for (const auto method_index : decl.method_decl_indices)
            if (method_index >= artifact.decls.size())
                return false;
    }

    for (size_t index = 0; index < artifact.templates.size(); ++index) {
        const auto &blueprint = artifact.templates[index];
        if (!validStringId(blueprint.name_id) ||
            blueprint.declaration_index >= artifact.decls.size() ||
            (blueprint.function_index != ~uint32_t{0} &&
             blueprint.function_index >= artifact.functions.size()) ||
            (blueprint.return_type_id != ~uint32_t{0} &&
             blueprint.return_type_id >= artifact.types.size()) ||
            blueprint.param_type_ids.size() != blueprint.param_name_ids.size() ||
            blueprint.field_name_ids.size() != blueprint.field_type_ids.size())
            return false;
        const auto &owner = artifact.decls[blueprint.declaration_index];
        if (owner.template_index != index)
            return false;
        for (const auto &param : blueprint.params) {
            if (!validStringId(param.name_id))
                return false;
            for (const auto bound_id : param.bound_type_ids)
                if (bound_id >= artifact.types.size())
                    return false;
        }
        for (const auto type_id : blueprint.param_type_ids)
            if (type_id >= artifact.types.size())
                return false;
        for (const auto name_id : blueprint.param_name_ids)
            if (!validStringId(name_id))
                return false;
        for (const auto name_id : blueprint.field_name_ids)
            if (!validStringId(name_id))
                return false;
        for (const auto type_id : blueprint.field_type_ids)
            if (type_id >= artifact.types.size())
                return false;
        std::vector<bool> seen_fields(blueprint.field_name_ids.size(), false);
        for (const auto field_index : blueprint.canonical_field_order) {
            if (field_index >= seen_fields.size() || seen_fields[field_index])
                return false;
            seen_fields[field_index] = true;
        }
        if (!blueprint.canonical_field_order.empty() &&
            blueprint.canonical_field_order.size() != seen_fields.size())
            return false;
        for (const auto method_index : blueprint.method_decl_indices)
            if (method_index >= artifact.decls.size())
                return false;
        for (const auto trait_id : blueprint.trait_type_ids) {
            if (trait_id >= artifact.types.size() ||
                artifact.types[trait_id].kind != cache::CompactTypeKind::Trait)
                return false;
        }
        if (blueprint.function_index != ~uint32_t{0}) {
            const auto &function = artifact.functions[blueprint.function_index];
            if (blueprint.return_type_id != function.return_type_id ||
                blueprint.param_type_ids != function.param_type_ids ||
                blueprint.param_name_ids != function.param_name_ids ||
                blueprint.is_variadic != function.is_variadic ||
                blueprint.is_state != function.is_state ||
                blueprint.is_foreign_c != function.is_foreign_c ||
                blueprint.uses_tailcc != function.uses_tailcc)
                return false;
        }
    }

    return true;
}

bool readFrontendIdentity(ByteReader &reader, cache::FrontendIdentity &identity) {
    return reader.readBlob(identity.dialect) && reader.readBlob(identity.implementation) &&
           reader.readBlob(identity.version) && reader.empty();
}

} // namespace

std::optional<cache::Artifact> Reader::read(std::string_view bytes) {
    if (bytes.size() < kFixedHeaderSize)
        return std::nullopt;

    ByteReader reader(bytes);
    uint32_t magic = 0, container_version = 0, hir_schema_version = 0;
    uint8_t endianness = 0, reserved = 0;
    uint16_t section_count = 0;
    uint32_t header_size = 0, checksum = 0;
    uint64_t required_features = 0, optional_features = 0;
    uint64_t source_fp_primary = 0, source_fp_secondary = 0;
    uint32_t semantic_profile_version = 0, public_abi_hi = 0, public_abi_lo = 0;
    uint32_t dep_count = 0, decl_count = 0, template_count = 0, fn_count = 0;
    uint32_t canonical_path_len = 0, module_name_len = 0, profile_name_len = 0;
    if (!reader.readU32(magic) || !reader.readU32(container_version) ||
        !reader.readU32(hir_schema_version) || !reader.readU8(endianness) ||
        !reader.readU8(reserved) || !reader.readU16(section_count) ||
        !reader.readU32(header_size) || !reader.readU32(checksum) ||
        !reader.readU64(required_features) || !reader.readU64(optional_features) ||
        !reader.readU64(source_fp_primary) || !reader.readU64(source_fp_secondary) ||
        !reader.readU32(semantic_profile_version) || !reader.readU32(public_abi_hi) ||
        !reader.readU32(public_abi_lo) || !reader.readU32(dep_count) ||
        !reader.readU32(decl_count) || !reader.readU32(template_count) ||
        !reader.readU32(fn_count) || !reader.readU32(canonical_path_len) ||
        !reader.readU32(module_name_len) || !reader.readU32(profile_name_len))
        return std::nullopt;

    if (magic != kMagic || container_version != kContainerVersion ||
        hir_schema_version != cache::kHirSchemaVersion || endianness != kEndianLittle ||
        reserved != 0 || header_size > bytes.size() || header_size < kFixedHeaderSize ||
        (required_features & cache::kRequiredHirFeatures) != cache::kRequiredHirFeatures ||
        (required_features & ~cache::kRequiredHirFeatures) != 0)
        return std::nullopt;

    cache::Artifact artifact;
    artifact.hir_schema_version       = hir_schema_version;
    artifact.required_features        = required_features;
    artifact.optional_features        = optional_features;
    artifact.source_fingerprint       = {source_fp_primary, source_fp_secondary};
    artifact.semantic_profile.version = semantic_profile_version;
    artifact.public_abi_hi            = public_abi_hi;
    artifact.public_abi_lo            = public_abi_lo;

    const auto readRawString = [&](uint32_t length, std::string &destination) {
        if (length > reader.remaining())
            return false;
        destination.assign(bytes.data() + reader.position(), length);
        for (uint32_t i = 0; i < length; ++i) {
            uint8_t ignored = 0;
            if (!reader.readU8(ignored))
                return false;
        }
        return true;
    };
    if (!readRawString(canonical_path_len, artifact.canonical_path) ||
        !readRawString(module_name_len, artifact.module_name) ||
        !readRawString(profile_name_len, artifact.semantic_profile.name) ||
        artifact.canonical_path.empty() || artifact.semantic_profile.name.empty())
        return std::nullopt;
    artifact.module_identity.path = artifact.canonical_path;

    if (!reader.canReadU32Count(dep_count, 16U))
        return std::nullopt;
    artifact.deps.resize(dep_count);
    for (auto &dep : artifact.deps)
        if (!reader.readBlob(dep.canonical_path) || !reader.readBlob(dep.import_key) ||
            !reader.readU32(dep.public_abi_hi) || !reader.readU32(dep.public_abi_lo))
            return std::nullopt;

    const uint64_t table_size = static_cast<uint64_t>(section_count) * kSectionEntrySize;
    if (reader.position() > header_size || table_size != header_size - reader.position() ||
        !reader.canReadU32Count(section_count, kSectionEntrySize))
        return std::nullopt;

    std::vector<SectionEntry> entries;
    entries.reserve(section_count);
    for (uint16_t index = 0; index < section_count; ++index) {
        SectionEntry entry;
        if (!reader.readU16(entry.id) || !reader.readU16(entry.flags) ||
            !reader.readU16(entry.schema_version) || !reader.readU16(entry.reserved) ||
            !reader.readU64(entry.offset) || !reader.readU64(entry.size))
            return std::nullopt;
        if (entry.id == 0 || entry.reserved != 0 || (entry.flags & ~kKnownSectionFlags) != 0 ||
            entry.offset < header_size || entry.offset > bytes.size() ||
            entry.size > bytes.size() - entry.offset || entry.schema_version == 0)
            return std::nullopt;
        if (std::any_of(entries.begin(), entries.end(),
                        [&entry](const SectionEntry &existing) { return existing.id == entry.id; }))
            return std::nullopt;
        entries.push_back(entry);
    }
    if (reader.position() != header_size)
        return std::nullopt;

    std::vector<SectionView> sections;
    sections.reserve(entries.size());
    for (const auto &entry : entries) {
        const bool known_required =
            std::any_of(kRequiredSections.begin(), kRequiredSections.end(),
                        [&entry](SectionId id) { return entry.id == static_cast<uint16_t>(id); });
        const bool known_optional =
            entry.id == static_cast<uint16_t>(SectionId::Instantiations) ||
            entry.id == static_cast<uint16_t>(SectionId::FrontendProvenance) ||
            entry.id == static_cast<uint16_t>(SectionId::Vir);
        if (!known_required && !known_optional && (entry.flags & kSectionFlagRequired) != 0)
            return std::nullopt;
        if (known_required && (entry.flags & kSectionFlagRequired) == 0)
            return std::nullopt;
        if (known_optional && (entry.flags & kSectionFlagRequired) != 0)
            return std::nullopt;

        const auto payload =
            bytes.substr(static_cast<size_t>(entry.offset), static_cast<size_t>(entry.size));
        sections.push_back({entry, payload});
    }

    std::sort(sections.begin(), sections.end(), [](const SectionView &a, const SectionView &b) {
        return a.entry.offset < b.entry.offset;
    });
    uint64_t previous_end = header_size;
    for (const auto &section : sections) {
        if (section.entry.offset < previous_end)
            return std::nullopt;
        previous_end = section.entry.offset + section.entry.size;
    }
    if (previous_end != bytes.size())
        return std::nullopt;

    if (checksumDependenciesAndSections(artifact, sections) != checksum)
        return std::nullopt;

    for (const auto required : kRequiredSections)
        if (findSection(sections, required) == nullptr)
            return std::nullopt;

    const auto decodeRequired = [&](SectionId id, uint16_t schema_version, auto decode) {
        const auto *section = findSection(sections, id);
        if (section == nullptr || section->entry.schema_version != schema_version)
            return false;
        ByteReader section_reader(section->payload);
        return decode(section_reader) && section_reader.empty();
    };
    if (!decodeRequired(SectionId::Metadata, kMetadataSectionSchemaVersion,
                        [&](ByteReader &r) { return decodeTypes(r, artifact); }) ||
        !decodeRequired(SectionId::Decls, kDeclsSectionSchemaVersion,
                        [&](ByteReader &r) { return decodeDecls(r, artifact); }) ||
        !decodeRequired(SectionId::Templates, kTemplatesSectionSchemaVersion,
                        [&](ByteReader &r) { return decodeTemplates(r, artifact); }) ||
        !decodeRequired(SectionId::Code, kSectionSchemaVersion,
                        [&](ByteReader &r) { return decodeCode(r, artifact); }) ||
        !decodeRequired(SectionId::Attrs, kAttrsSectionSchemaVersion,
                        [&](ByteReader &r) { return decodeAttrs(r, artifact); }))
        return std::nullopt;

    if (artifact.decls.size() != decl_count || artifact.templates.size() != template_count ||
        artifact.functions.size() != fn_count)
        return std::nullopt;

    for (const auto &section : sections) {
        if (section.entry.id == static_cast<uint16_t>(SectionId::Instantiations)) {
            if (section.entry.schema_version != kSectionSchemaVersion)
                continue;
            ByteReader section_reader(section.payload);
            if (!decodeInstantiations(section_reader, artifact) || !section_reader.empty())
                return std::nullopt;
        } else if (section.entry.id == static_cast<uint16_t>(SectionId::FrontendProvenance)) {
            if (section.entry.schema_version != kSectionSchemaVersion)
                continue;
            ByteReader section_reader(section.payload);
            if (!readFrontendIdentity(section_reader, artifact.frontend_identity))
                return std::nullopt;
        } else if (section.entry.id == static_cast<uint16_t>(SectionId::Debug)) {
            if (section.entry.schema_version != kDebugSectionSchemaVersion)
                continue;
            ByteReader section_reader(section.payload);
            if (!decodeDebug(section_reader, artifact) || !section_reader.empty())
                return std::nullopt;
        } else if (section.entry.id > static_cast<uint16_t>(SectionId::FrontendProvenance)) {
            cache::Artifact::OptionalSection optional;
            optional.id             = section.entry.id;
            optional.schema_version = section.entry.schema_version;
            optional.payload.assign(reinterpret_cast<const uint8_t *>(section.payload.data()),
                                    reinterpret_cast<const uint8_t *>(section.payload.data()) +
                                        section.payload.size());
            artifact.optional_sections.push_back(std::move(optional));
        }
    }

    if (!validateArtifactReferences(artifact))
        return std::nullopt;
    return artifact;
}

} // namespace zith::zirl
