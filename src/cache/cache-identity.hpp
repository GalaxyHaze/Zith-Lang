#pragma once

#include <cstdint>
#include <string>

namespace zith::cache {

inline constexpr uint32_t kHirSchemaVersion = 3;

inline constexpr uint64_t kFeatureCanonicalReferences = uint64_t{1} << 0u;
inline constexpr uint64_t kFeatureHirDeclarations = uint64_t{1} << 1u;
inline constexpr uint64_t kFeatureSafetyObligations = uint64_t{1} << 2u;
inline constexpr uint64_t kRequiredHirFeatures = kFeatureCanonicalReferences |
                                                  kFeatureHirDeclarations |
                                                  kFeatureSafetyObligations;

inline constexpr uint64_t kOptionalFeatureInstantiationSummary = uint64_t{1} << 0u;
inline constexpr uint64_t kOptionalFeatureFrontendProvenance = uint64_t{1} << 1u;
inline constexpr uint64_t kOptionalFeatureDerivedVir = uint64_t{1} << 2u;
inline constexpr uint64_t kOptionalFeatureTestDebug = uint64_t{1} << 3u;
inline constexpr uint8_t kFunctionSignatureVariadic = 1U << 0U;
inline constexpr uint8_t kFunctionSignatureState = 1U << 1U;
inline constexpr uint8_t kFunctionSignatureForeignC = 1U << 2U;
inline constexpr uint8_t kFunctionSignatureTailCC = 1U << 3U;

// These identities deliberately distinguish the producer from the semantic
// contract. A different frontend may produce reusable HIR when its profile
// and canonical module match.
struct FrontendIdentity {
    std::string dialect;
    std::string implementation;
    std::string version;

    bool operator==(const FrontendIdentity &) const = default;
};

struct SemanticCompatibilityProfile {
    std::string name;
    uint32_t version = 0;

    bool operator==(const SemanticCompatibilityProfile &) const = default;
};

struct CanonicalModuleIdentity {
    std::string path;

    bool operator==(const CanonicalModuleIdentity &) const = default;
};

struct ContentFingerprint {
    uint64_t primary   = 0;
    uint64_t secondary = 0;

    bool operator==(const ContentFingerprint &) const = default;
};

inline const SemanticCompatibilityProfile kSharedHirProfile{"zith-shared-hir", 1};

} // namespace zith::cache
