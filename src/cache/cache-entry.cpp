#include "cache/cache-entry.hpp"

namespace zith::cache {

bool validateArtifact(const Artifact &art, const CanonicalModuleIdentity &module_identity,
                      const SemanticCompatibilityProfile &semantic_profile,
                      const ContentFingerprint &fingerprint) {
    if (module_identity.path.empty() || art.module_identity != module_identity ||
        art.canonical_path != module_identity.path)
        return false;
    if (art.semantic_profile != semantic_profile ||
        art.hir_schema_version != kHirSchemaVersion ||
        (art.required_features & kRequiredHirFeatures) != kRequiredHirFeatures ||
        (art.required_features & ~kRequiredHirFeatures) != 0)
        return false;
    if (art.source_fingerprint != fingerprint)
        return false;
    return true;
}

std::optional<uint32_t> resolveFunctionReference(const Artifact &artifact, uint32_t name_id,
                                                 uint32_t signature_type_id) {
    if (name_id >= artifact.strings.size() || signature_type_id >= artifact.types.size())
        return std::nullopt;

    const auto &signature = artifact.types[signature_type_id];
    constexpr uint8_t known_signature_flags =
        kFunctionSignatureVariadic | kFunctionSignatureState | kFunctionSignatureForeignC |
        kFunctionSignatureTailCC;
    if (signature.kind != CompactTypeKind::Fn ||
        (signature.flags & ~known_signature_flags) != 0)
        return std::nullopt;

    const auto &name = artifact.strings[name_id];
    std::optional<uint32_t> match;
    for (size_t index = 0; index < artifact.functions.size(); ++index) {
        const auto &function = artifact.functions[index];
        const auto function_name =
            function.name.empty() && function.name_id < artifact.strings.size()
                ? std::string_view(artifact.strings[function.name_id])
                : std::string_view(function.name);
        if (function_name != name || function.signature_type_id != signature_type_id ||
            function.return_type_id != signature.ref0 ||
            function.param_type_ids != signature.args)
            continue;

        const uint8_t function_flags =
            (function.is_variadic ? kFunctionSignatureVariadic : 0U) |
            (function.is_state ? kFunctionSignatureState : 0U) |
            (function.is_foreign_c ? kFunctionSignatureForeignC : 0U) |
            (function.uses_tailcc ? kFunctionSignatureTailCC : 0U);
        if (function_flags != signature.flags)
            continue;
        if (match.has_value())
            return std::nullopt;
        match = static_cast<uint32_t>(index);
    }
    return match;
}

} // namespace zith::cache
