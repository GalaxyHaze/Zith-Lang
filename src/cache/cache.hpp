#pragma once

#include "cache/cache-entry.hpp"
#include "cache/cache-types.hpp"
#include "cache/manifest.hpp"
#include "session/frontend-context.hpp" // session::CacheKey, session::ContentFingerprint
#include "types/type-id.hpp"
#include "zirl/zirl-header.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace zith::cache {

struct StoreMetrics {
    size_t hits      = 0;
    size_t misses    = 0;
    size_t invalid   = 0;
    size_t writes    = 0;
    size_t evictions = 0;
};

/// Describes a persisted canonical opaque mapping that the current registry
/// cannot reproduce.  Callers use this to produce an actionable, deterministic
/// cache-evolution diagnostic instead of a generic invalidation notice.
struct CanonicalDivergence {
    types::TypeCanonicalId canonical_id;
    uint32_t persisted_tag   = 0;
    uint32_t current_tag     = 0;
    bool field_order_changed = false;
    bool registry_conflicts  = false;
    std::string recovery_command;
};

struct CanonicalIdLess {
    [[nodiscard]] bool operator()(const types::TypeCanonicalId &a,
                                  const types::TypeCanonicalId &b) const noexcept {
        return a.hi != b.hi ? a.hi < b.hi : a.lo < b.lo;
    }
};

// Persistent per-module artifact store.  Owns the on-disk cache directory and a
// Manifest for reverse-dependency invalidation.  Thread-safe.
class Store {
public:
    Store(std::string cache_root, const session::CacheKey &cache_key);

    /// Return the project-local opaque tag for `canonical_id`, assigning a new
    /// stable tag when the id is seen for the first time and persisting it to
    /// `<cacheRoot>/canonical-any`.
    uint32_t assignCanonicalId(const types::TypeCanonicalId &canonical_id);

    /// Returns the currently assigned canonical tag, without assigning a new
    /// id when the canonical is not present.  Used by hydration checks.
    [[nodiscard]] std::optional<uint32_t>
    lookupCanonicalId(const types::TypeCanonicalId &canonical_id) const;

    /// Checks whether a persisted mapping still matches the project registry.
    /// When it does not, the divergence carries the current/persisted ids so a
    /// hydration diagnostic can identify the exact canonical type.
    [[nodiscard]] CanonicalDivergence
    checkCanonicalMapping(const types::TypeCanonicalId &canonical_id, uint32_t persisted_tag) const;

    // Try to load and validate the artifact for `canonical_path` whose source
    // fingerprint is `fp`.  Returns the artifact on a full hit, or std::nullopt
    // on any miss/invalidation/corruption.  Increments metrics accordingly.
    [[nodiscard]] std::optional<Artifact> load(std::string_view canonical_path,
                                               const session::ContentFingerprint &fp);

    // Try to load and validate the artifact for `canonical_path` whose source
    // fingerprint is `fp`.  Returns a fully validated CacheEntry on a full hit,
    // or std::nullopt on any miss/invalidation/corruption.
    [[nodiscard]] std::optional<CacheEntry> loadEntry(std::string_view canonical_path,
                                                      const session::ContentFingerprint &fp);

    // Remove an artifact that failed validation so later loads do not repeat
    // the same parse/check work. Best-effort and reflected in metrics.
    void dropInvalid(std::string_view canonical_path);

    // Persist `artifact` to disk and update the manifest.  Best-effort: disk
    // write failures are swallowed and only reflected in metrics.
    void store(const Artifact &artifact);

    // Invalidate a module and all of its transitive dependents.
    void invalidate(std::string_view canonical_path);

    [[nodiscard]] StoreMetrics metrics() const;
    [[nodiscard]] std::optional<ManifestEntry> manifestEntry(std::string_view canonical_path) const;

    [[nodiscard]] const std::string &root() const noexcept {
        return root_;
    }

private:
    std::string root_;
    uint32_t cache_key_hash_ = 0;
    Manifest manifest_;
    mutable std::mutex canonical_mutex_;
    std::map<types::TypeCanonicalId, uint32_t, CanonicalIdLess> canonical_registry_;
    std::map<uint32_t, types::TypeCanonicalId> canonical_by_tag_;
    mutable std::mutex metrics_mutex_;
    StoreMetrics metrics_;

    [[nodiscard]] std::string artifactPath(std::string_view canonical_path) const;
    [[nodiscard]] std::string canonicalRegistryPath() const;
    void loadCanonicalRegistry();
    void saveCanonicalRegistryLocked();
    void bumpHits() {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.hits;
    }
    void bumpMisses() {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.misses;
    }
    void bumpInvalid() {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.invalid;
    }
    void bumpWrites() {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        ++metrics_.writes;
    }
};

} // namespace zith::cache
