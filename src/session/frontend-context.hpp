#pragma once

#include "ast/ast-builder.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/diagnostic.hpp"
#include "lexer/token.hpp"
#include "memory/result.hpp"
#include "memory/source-map.hpp"
#include "memory/string-interner.hpp"
#include "parser/scan-result.hpp"
#include "symbols/symbol-table.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace zith::session {

/// A stable, process-local content identity.  Two independent FNV streams make
/// accidental collisions impractical while keeping the key cheap to copy.
struct ContentFingerprint {
    uint64_t primary   = 0;
    uint64_t secondary = 0;

    constexpr bool operator==(const ContentFingerprint &) const noexcept = default;
    constexpr bool operator!=(const ContentFingerprint &) const noexcept = default;

    [[nodiscard]] std::string toString() const;
    [[nodiscard]] static ContentFingerprint fromText(std::string_view text) noexcept;
};

/// Immutable text registered by a workspace before a frontend worker sees it.
struct SourceRecord {
    memory::FileId id = 0;
    std::string canonicalPath;
    ContentFingerprint fingerprint;
    std::string text;
};

/// Append-only catalog shared by all frontend snapshots in a workspace.
///
/// `SourceMap` is intentionally not shared: it remains a mutable parser detail
/// inside each ModuleStorage.  The catalog is the authoritative identity and
/// lifetime owner for source text exposed through snapshots.
class SourceCatalog {
public:
    using SourcePtr = std::shared_ptr<const SourceRecord>;

    SourceCatalog() = default;

    [[nodiscard]] static std::string canonicalPath(std::string_view path);
    [[nodiscard]] SourcePtr registerSource(std::string path, std::string text);
    [[nodiscard]] memory::Result<SourcePtr> loadFile(std::string_view path);
    [[nodiscard]] SourcePtr find(memory::FileId id) const;
    [[nodiscard]] SourcePtr find(std::string_view canonical_path,
                                 ContentFingerprint fingerprint) const;
    [[nodiscard]] size_t size() const noexcept;

private:
    struct SourceKey {
        std::string path;
        ContentFingerprint fingerprint;

        bool operator==(const SourceKey &) const noexcept = default;
    };

    struct SourceKeyHash {
        size_t operator()(const SourceKey &key) const noexcept;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<SourceKey, SourcePtr, SourceKeyHash> by_key_;
    std::vector<SourcePtr> by_id_;
};

/// Immutable fields which affect a cached frontend artifact.
struct CacheKey {
    std::string compilerVersion;
    std::string targetTriple;
    std::string parseFlags;
    std::string visibilityFlags;
    std::vector<std::string> includeRoots;
    std::vector<std::string> stdlibRoots;

    [[nodiscard]] std::string identity() const;
    bool operator==(const CacheKey &) const noexcept = default;
};

struct FrontendConfig {
    size_t maxFrontendWorkers = 1;
    std::string workspaceRoot;
    std::string compilerVersion;
    std::string targetTriple;
    std::string parseFlags;
    std::string visibilityFlags;
    std::vector<std::string> includeRoots;
    std::vector<std::string> stdlibRoots;

    [[nodiscard]] CacheKey cacheKey() const;
};

/// Fixed-size executor used only for independent module frontend work.
/// It deliberately has no priority queue; LSP request scheduling belongs to a
/// higher layer and must remain independent from this module limit.
class ModuleExecutor {
public:
    explicit ModuleExecutor(size_t requested_workers = 1);
    ~ModuleExecutor();

    ModuleExecutor(const ModuleExecutor &)            = delete;
    ModuleExecutor &operator=(const ModuleExecutor &) = delete;

    [[nodiscard]] size_t workerCount() const noexcept {
        return worker_count_;
    }

    [[nodiscard]] static size_t normalizeWorkerCount(size_t requested) noexcept;

    template <typename Function>
    auto submit(Function &&function) -> std::future<std::invoke_result_t<Function>> {
        using Result = std::invoke_result_t<Function>;
        auto task =
            std::make_shared<std::packaged_task<Result()>>(std::forward<Function>(function));
        auto future = task->get_future();

#ifdef ZITH_IS_WASM
        (*task)();
#else
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.emplace_back([task]() { (*task)(); });
        }
        condition_.notify_one();
#endif
        return future;
    }

private:
#ifndef ZITH_IS_WASM
    void workerLoop();
#endif

    size_t worker_count_ = 1;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<std::function<void()>> tasks_;
    bool stopping_ = false;
    std::vector<std::thread> workers_;
};

using ModuleKey = std::string;

struct ImportRequest {
    std::vector<std::string> path;
    bool isFrom   = false;
    bool isExport = false;
    bool isAsset  = false;
    int32_t depth = 1;
    memory::Span span{};

    [[nodiscard]] std::string importKey() const;
};

struct ModuleDiagnostic {
    diagnostics::Severity severity = diagnostics::Severity::Error;
    uint32_t code                  = 0;
    std::string message;
    memory::FileId file      = 0;
    memory::ByteOffset start = 0;
    memory::ByteOffset end   = 0;
};

struct LocalSymbolInfo {
    symbols::SymId id = symbols::kInvalidSym;
    std::string name;
    symbols::SymbolVisibility visibility = symbols::SymbolVisibility::Private;
    symbols::SymKind kind                = symbols::SymKind::Variable;
    memory::Span span{};
};

struct ModuleTimings {
    double lexMs    = 0.0;
    double scanMs   = 0.0;
    double expandMs = 0.0;
};

/// Per-module storage.  It is populated by one worker then published as const.
/// No arena, interner, source map, or diagnostic engine is shared with another
/// ModuleStorage instance.
class ModuleStorage {
public:
    explicit ModuleStorage(SourceCatalog::SourcePtr source);

    ModuleStorage(const ModuleStorage &)            = delete;
    ModuleStorage &operator=(const ModuleStorage &) = delete;

    [[nodiscard]] const SourceCatalog::SourcePtr &source() const noexcept {
        return source_;
    }
    [[nodiscard]] const memory::SourceMap &sourceMap() const noexcept {
        return source_map_;
    }
    [[nodiscard]] const lexer::TokenStream &tokens() const noexcept {
        return tokens_;
    }
    [[nodiscard]] const ast::AstBuilder &builder() const noexcept {
        return builder_;
    }
    [[nodiscard]] const symbols::SymbolTable &symbols() const noexcept {
        return symbols_;
    }
    [[nodiscard]] const ast::ProgramNode &program() const noexcept {
        return program_;
    }
    [[nodiscard]] const parser::ScanResult &scanResult() const noexcept {
        return scan_result_;
    }
    [[nodiscard]] const diagnostics::DiagnosticEngine &diagnostics() const noexcept {
        return diagnostics_;
    }
    [[nodiscard]] const memory::StringInterner &interner() const noexcept {
        return interner_;
    }
    [[nodiscard]] size_t arenaBytes() const noexcept;

private:
    friend class FrontendContext;

    SourceCatalog::SourcePtr source_;
    memory::Arena scratch_arena_;
    memory::Arena ast_arena_;
    memory::Arena symbol_arena_;
    memory::SourceMap source_map_;
    diagnostics::DiagnosticEngine diagnostics_;
    memory::StringInterner interner_;
    ast::AstBuilder builder_;
    symbols::SymbolTable symbols_;
    lexer::TokenStream tokens_{};
    ast::ProgramNode program_;
    parser::ScanResult scan_result_;
};

struct ModuleArtifact {
    ModuleKey key;
    memory::FileId fileId = 0;
    ContentFingerprint fingerprint;
    std::shared_ptr<const ModuleStorage> storage;
    std::vector<ImportRequest> imports;
    std::vector<LocalSymbolInfo> publicSymbols;
    std::vector<LocalSymbolInfo> moduleSymbols;
    std::vector<ModuleDiagnostic> diagnostics;
    ModuleTimings timings;

    [[nodiscard]] bool hasErrors() const noexcept;
};

using ModuleArtifactPtr = std::shared_ptr<const ModuleArtifact>;

struct ModuleSymbolRef {
    ModuleKey module;
    symbols::SymId localSymbol = symbols::kInvalidSym;
};

struct MergedSymbol {
    std::string name;
    symbols::SymbolVisibility visibility = symbols::SymbolVisibility::Private;
    symbols::SymKind kind                = symbols::SymKind::Variable;
    ModuleSymbolRef origin;
    memory::Span span{};
};

struct SnapshotMetrics {
    size_t moduleCount   = 0;
    size_t artifactBytes = 0;
    size_t cacheHits     = 0;
    size_t cacheMisses   = 0;
    double lexMs         = 0.0;
    double scanMs        = 0.0;
    double expandMs      = 0.0;
};

/// Immutable analysis view used by editor features.  All references held by a
/// snapshot stay valid after a cache entry is invalidated.
class CompilationSnapshot {
public:
    CompilationSnapshot(std::shared_ptr<const SourceCatalog> catalog, CacheKey cache_key,
                        std::vector<ModuleArtifactPtr> modules,
                        std::vector<MergedSymbol> merged_symbols,
                        std::vector<ModuleDiagnostic> diagnostics, SnapshotMetrics metrics);

    [[nodiscard]] const SourceCatalog &sourceCatalog() const noexcept {
        return *catalog_;
    }
    [[nodiscard]] const CacheKey &cacheKey() const noexcept {
        return cache_key_;
    }
    [[nodiscard]] const std::vector<ModuleArtifactPtr> &modules() const noexcept {
        return modules_;
    }
    [[nodiscard]] const std::vector<MergedSymbol> &mergedSymbols() const noexcept {
        return merged_symbols_;
    }
    [[nodiscard]] const std::vector<ModuleDiagnostic> &diagnostics() const noexcept {
        return diagnostics_;
    }
    [[nodiscard]] const SnapshotMetrics &metrics() const noexcept {
        return metrics_;
    }
    [[nodiscard]] const ModuleArtifact *findModule(std::string_view key) const noexcept;
    [[nodiscard]] bool hasErrors() const noexcept;

private:
    std::shared_ptr<const SourceCatalog> catalog_;
    CacheKey cache_key_;
    std::vector<ModuleArtifactPtr> modules_;
    std::vector<MergedSymbol> merged_symbols_;
    std::vector<ModuleDiagnostic> diagnostics_;
    SnapshotMetrics metrics_;
};

struct ModuleCacheMetrics {
    size_t hits        = 0;
    size_t misses      = 0;
    size_t invalidated = 0;
    size_t entries     = 0;
};

/// Thread-safe cache of the newest module artifact per canonical path and
/// CacheKey.  A snapshot owns old artifacts after replacement, not the cache.
class ModuleCache {
public:
    ModuleCache() = default;

    ModuleCache(const ModuleCache &)            = delete;
    ModuleCache &operator=(const ModuleCache &) = delete;

    [[nodiscard]] std::shared_future<ModuleArtifactPtr>
    getOrBuild(const CacheKey &cache_key, SourceCatalog::SourcePtr source, ModuleExecutor &executor,
               std::function<ModuleArtifactPtr()> build);

    /// Records which content revision is current for a path.  A new revision
    /// invalidates that module and all reverse dependents before it can publish.
    void noteSource(const SourceCatalog::SourcePtr &source);
    void updateDependencies(const ModuleKey &module, std::vector<ModuleKey> dependencies);
    void invalidate(std::string_view canonical_path);
    [[nodiscard]] ModuleCacheMetrics metrics() const;

private:
    struct InFlight {
        std::shared_future<ModuleArtifactPtr> future;
        uint64_t epoch = 0;
    };

    [[nodiscard]] static std::string bucketKey(const CacheKey &cache_key, std::string_view path);
    [[nodiscard]] static std::string inFlightKey(const CacheKey &cache_key,
                                                 const SourceRecord &source);
    [[nodiscard]] std::shared_future<ModuleArtifactPtr>
    readyFuture(ModuleArtifactPtr artifact) const;
    void invalidateLocked(const ModuleKey &module);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ModuleArtifactPtr> artifacts_;
    std::unordered_map<std::string, InFlight> in_flight_;
    std::unordered_map<ModuleKey, ContentFingerprint> current_fingerprints_;
    std::unordered_map<ModuleKey, uint64_t> epochs_;
    std::unordered_map<ModuleKey, std::unordered_set<ModuleKey>> dependencies_;
    std::unordered_map<ModuleKey, std::unordered_set<ModuleKey>> reverse_dependencies_;
    ModuleCacheMetrics metrics_;
};

struct FrontendMetrics {
    ModuleCacheMetrics cache;
    size_t maxFrontendWorkers = 1;
};

/// Shared, workspace-scoped frontend state.  It has no global mutable parser
/// state and is suitable for use by several LSP request workers.
class FrontendContext {
public:
    explicit FrontendContext(FrontendConfig config = {});

    FrontendContext(const FrontendContext &)            = delete;
    FrontendContext &operator=(const FrontendContext &) = delete;

    [[nodiscard]] const FrontendConfig &config() const noexcept {
        return config_;
    }
    [[nodiscard]] const CacheKey &cacheKey() const noexcept {
        return cache_key_;
    }
    [[nodiscard]] size_t maxFrontendWorkers() const noexcept {
        return executor_.workerCount();
    }
    [[nodiscard]] FrontendMetrics metrics() const;

    [[nodiscard]] memory::Result<std::shared_ptr<const CompilationSnapshot>>
    analyzeFile(std::string_view path);
    [[nodiscard]] memory::Result<std::shared_ptr<const CompilationSnapshot>>
    analyzeText(std::string_view path, std::string text);

    /// Open-document content takes precedence over disk until removed.
    void setOverlay(std::string_view path, std::string text);
    void removeOverlay(std::string_view path);
    void invalidatePath(std::string_view path);

    /// Prebuild configured stdlib roots before accepting analysis requests.
    /// Callers such as an LSP initialize handler own the one-time policy.
    [[nodiscard]] memory::Result<bool> initializeStdlib();

    [[nodiscard]] std::shared_ptr<const SourceCatalog> sourceCatalog() const noexcept {
        return catalog_;
    }

private:
    struct PendingModule {
        ModuleKey key;
        SourceCatalog::SourcePtr source;
    };

    struct ResolvedImport {
        std::vector<ModuleKey> modules;
        bool found = false;
    };

    [[nodiscard]] memory::Result<SourceCatalog::SourcePtr> sourceForPath(std::string_view path);
    [[nodiscard]] memory::Result<std::shared_ptr<const CompilationSnapshot>>
    analyze(SourceCatalog::SourcePtr root_source);
    [[nodiscard]] ModuleArtifactPtr buildModule(SourceCatalog::SourcePtr source) const;
    [[nodiscard]] std::vector<std::string> visibleRootsFor(std::string_view root_path) const;
    [[nodiscard]] ResolvedImport resolveImport(const ModuleArtifact &artifact,
                                               const ImportRequest &request,
                                               const std::vector<std::string> &visible_roots) const;
    [[nodiscard]] static std::vector<std::string>
    collectDirectoryModules(std::string_view directory, int32_t depth);
    static void sortDiagnostics(std::vector<ModuleDiagnostic> &diagnostics,
                                const SourceCatalog &catalog);
    static std::vector<MergedSymbol> mergeSymbols(const std::vector<ModuleArtifactPtr> &modules);
    static void
    appendCycleDiagnostics(const std::vector<ModuleArtifactPtr> &modules,
                           const std::map<ModuleKey, std::vector<ModuleKey>> &dependencies,
                           std::vector<ModuleDiagnostic> &diagnostics);

    FrontendConfig config_;
    CacheKey cache_key_;
    std::shared_ptr<SourceCatalog> catalog_;
    ModuleCache cache_;
    ModuleExecutor executor_;
    mutable std::shared_mutex overlay_mutex_;
    std::unordered_map<std::string, std::string> overlays_;
    std::mutex stdlib_mutex_;
    bool stdlib_initialized_ = false;
};

} // namespace zith::session
