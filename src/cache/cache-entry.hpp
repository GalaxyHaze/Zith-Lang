#pragma once

#include "cache/cache-types.hpp"

#include <cstdint>
#include <optional>

namespace zith::cache {

enum class CacheEntryState : uint8_t {
    Invalid,
    Hydrating,
    Hydrated,
};

// A validated, in-memory cache entry produced by Store::loadEntry.
// Holds the decoded artifact together with the source fingerprint that was
// validated, plus a hydration-state marker used by CompilationSession to
// coordinate short-circuit stages.
struct CacheEntry {
    Artifact artifact;
    ContentFingerprint fingerprint;
    CacheEntryState state = CacheEntryState::Invalid;
};

// Validate the canonical module and semantic contract of a persisted artifact.
// Extracted from Store so CacheEntry consumers can re-validate without
// reaching into a Store instance.
[[nodiscard]] bool validateArtifact(const Artifact &art,
                                    const CanonicalModuleIdentity &module_identity,
                                    const SemanticCompatibilityProfile &semantic_profile,
                                    const ContentFingerprint &fingerprint);

// Resolve a canonical artifact-local name/signature pair without depending on
// a process-local HIR function index.
[[nodiscard]] std::optional<uint32_t>
resolveFunctionReference(const Artifact &artifact, uint32_t name_id,
                         uint32_t signature_type_id);

} // namespace zith::cache
