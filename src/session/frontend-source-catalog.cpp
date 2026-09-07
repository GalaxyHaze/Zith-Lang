#include "session/frontend-context-internal.hpp"
#include "session/frontend-context.hpp"

#include "cinterop/c-header.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace zith::session {

namespace fs = std::filesystem;

constexpr uint64_t kFnvOffset    = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime     = 1099511628211ULL;
constexpr uint64_t kSecondOffset = 7809847782465536322ULL;
constexpr uint64_t kSecondPrime  = 14029467366897019727ULL;

struct FnvParameters {
    uint64_t offset;
    uint64_t prime;
};

[[nodiscard]] uint64_t hashText(std::string_view text, FnvParameters parameters) noexcept {
    uint64_t hash = parameters.offset;
    for (const char character : text) {
        hash ^= static_cast<unsigned char>(character);
        hash *= parameters.prime;
    }
    return hash;
}

[[nodiscard]] std::string stableRoot(std::string_view path) {
    if (path.empty())
        return {};
    return SourceCatalog::canonicalPath(path);
}

void normalizeRoots(std::vector<std::string> &roots) {
    std::vector<std::string> normalized;
    normalized.reserve(roots.size());
    for (const auto &root : roots) {
        auto canonical = stableRoot(root);
        if (canonical.empty() ||
            std::find(normalized.begin(), normalized.end(), canonical) != normalized.end())
            continue;
        normalized.push_back(std::move(canonical));
    }
    roots = std::move(normalized);
}

void applyConfig(FrontendConfig &config) {
    config.workspaceRoot = stableRoot(config.workspaceRoot);
    normalizeRoots(config.includeRoots);
    normalizeRoots(config.stdlibRoots);
    normalizeRoots(config.assetRoots);
    if (config.useSystemIncludeRoots && config.systemIncludeRoots.empty())
        config.systemIncludeRoots =
            cinterop::systemIncludeDirs(config.targetTriple, config.sysroot);
    if (!config.useSystemIncludeRoots)
        config.systemIncludeRoots.clear();
    normalizeRoots(config.systemIncludeRoots);
}

std::string ContentFingerprint::toString() const {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << primary << std::setw(16)
           << secondary;
    return output.str();
}

ContentFingerprint ContentFingerprint::fromText(const std::string_view text) noexcept {
    return {
        hashText(text, {kFnvOffset, kFnvPrime}),
        hashText(text, {kSecondOffset, kSecondPrime}),
    };
}

std::string SourceCatalog::canonicalPath(const std::string_view path) {
#ifdef ZITH_IS_WASM
    return std::string(path);
#else
    std::error_code error;
    auto absolute = fs::absolute(fs::path(path), error);
    if (error)
        return fs::path(path).lexically_normal().generic_string();

    auto canonical = fs::weakly_canonical(absolute, error);
    if (error)
        return absolute.lexically_normal().generic_string();
    return canonical.generic_string();
#endif
}

size_t SourceCatalog::SourceKeyHash::operator()(const SourceKey &key) const noexcept {
    const auto path_hash = std::hash<std::string>{}(key.path);
    const auto first     = std::hash<uint64_t>{}(key.fingerprint.primary);
    const auto second    = std::hash<uint64_t>{}(key.fingerprint.secondary);
    return path_hash ^ (first << 1U) ^ (second << 7U);
}

SourceCatalog::SourcePtr SourceCatalog::registerSource(std::string path, std::string text) {
    path                   = canonicalPath(path);
    const auto fingerprint = ContentFingerprint::fromText(text);
    SourceKey key{path, fingerprint};

    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (const auto *existing = by_key_.get(key))
        return *existing;

    if (by_id_.size() >= static_cast<size_t>(std::numeric_limits<memory::FileId>::max()))
        return {};

    const auto id = static_cast<memory::FileId>(by_id_.size());
    auto source   = std::make_shared<const SourceRecord>(
        SourceRecord{id, std::move(path), fingerprint, std::move(text)});
    by_key_.insert(std::move(key), source);
    by_id_.push_back(source);
    return source;
}

memory::Result<SourceCatalog::SourcePtr> SourceCatalog::loadFile(const std::string_view path) {
#ifdef ZITH_IS_WASM
    (void)path;
    return memory::Error{"loading source files is not available on WASM"};
#else
    const auto canonical = canonicalPath(path);
    std::ifstream input(canonical, std::ios::binary);
    if (!input)
        return memory::Error{"failed to load '" + canonical + "'"};

    std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    return registerSource(canonical, std::move(content));
#endif
}

SourceCatalog::SourcePtr SourceCatalog::find(const memory::FileId id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (id >= by_id_.size())
        return {};
    return by_id_[id];
}

SourceCatalog::SourcePtr SourceCatalog::find(const std::string_view canonical_path,
                                             const ContentFingerprint fingerprint) const {
    const SourceKey key{canonicalPath(canonical_path), fingerprint};
    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (const auto *existing = by_key_.get(key))
        return *existing;
    return {};
}

size_t SourceCatalog::size() const noexcept {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return by_id_.size();
}

std::string CacheKey::identity() const {
    std::ostringstream output;
    // The published module shape changed from legacy parser objects to frontend artifacts.
    output << "frontend-artifact-v3\n"
           << workspaceRoot << '\n'
           << compilerVersion << '\n'
           << targetTriple << '\n'
           << parseFlags << '\n'
           << visibilityFlags << '\n'
           << sysroot << '\n';
    for (const auto &root : includeRoots)
        output << "I:" << root << '\n';
    for (const auto &root : stdlibRoots)
        output << "S:" << root << '\n';
    for (const auto &root : systemIncludeRoots)
        output << "Y:" << root << '\n';
    for (const auto &define : cDefines)
        output << "D:" << define << '\n';
    return output.str();
}

CacheKey FrontendConfig::cacheKey() const {
    CacheKey key{
        workspaceRoot, compilerVersion, targetTriple, parseFlags,         visibilityFlags,
        sysroot,       includeRoots,    stdlibRoots,  systemIncludeRoots, cDefines,
    };
    normalizeRoots(key.includeRoots);
    normalizeRoots(key.stdlibRoots);
    normalizeRoots(key.systemIncludeRoots);
    std::sort(key.cDefines.begin(), key.cDefines.end());
    key.cDefines.erase(std::unique(key.cDefines.begin(), key.cDefines.end()), key.cDefines.end());
    return key;
}

} // namespace zith::session
