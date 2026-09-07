#include "session/frontend-context-internal.hpp"
#include "session/frontend-context.hpp"

#include <algorithm>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zith::session {

std::string joinPath(const std::vector<std::string> &path) {
    std::string result;
    for (size_t index = 0; index < path.size(); ++index) {
        if (index != 0U)
            result += '/';
        result += path[index];
    }
    return result;
}

size_t ModuleExecutor::normalizeWorkerCount(const size_t requested) noexcept {
#ifdef ZITH_IS_WASM
    (void)requested;
    return 1;
#else
    const auto hardware = std::thread::hardware_concurrency();
    const auto maximum  = hardware == 0U ? size_t{1} : static_cast<size_t>(hardware);
    const auto minimum  = requested == 0U ? size_t{1} : requested;
    return std::min(minimum, maximum);
#endif
}

ModuleExecutor::ModuleExecutor(const size_t requested_workers)
    : worker_count_(normalizeWorkerCount(requested_workers)) {
#ifndef ZITH_IS_WASM
    workers_.reserve(worker_count_);
    for (size_t worker = 0; worker < worker_count_; ++worker)
        workers_.emplace_back([this]() { workerLoop(); });
#endif
}

ModuleExecutor::~ModuleExecutor() {
#ifndef ZITH_IS_WASM
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    for (auto &worker : workers_)
        worker.join();
#endif
}

#ifndef ZITH_IS_WASM
void ModuleExecutor::workerLoop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
            if (stopping_ && tasks_.empty())
                return;
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task();
    }
}
#endif

std::string ImportRequest::importKey() const {
    std::string result;
    for (size_t index = 0; index < path.size(); ++index) {
        if (index != 0U)
            result += '/';
        result += path[index];
    }
    return result;
}

void FrontendContext::invalidatePath(const std::string_view path) {
    cache_.invalidate(SourceCatalog::canonicalPath(path));
}

bool ModuleArtifact::hasErrors() const noexcept {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
                       [](const ModuleDiagnostic &diagnostic) {
                           return diagnostic.severity == diagnostics::Severity::Error ||
                                  diagnostic.severity == diagnostics::Severity::Bug;
                       });
}

CompilationSnapshot::CompilationSnapshot(
    std::shared_ptr<const SourceCatalog> catalog, CacheKey cache_key, ModuleKey root_module_key,
    std::vector<ModuleArtifactPtr> modules, std::vector<MergedSymbol> merged_symbols,
    std::vector<ImportEdge> import_graph,
    std::vector<std::shared_ptr<const cinterop::CHeaderArtifact>> c_headers,
    std::vector<ModuleResolution> resolutions, std::vector<ModuleDiagnostic> diagnostics,
    SnapshotMetrics metrics)
    : catalog_(std::move(catalog)), cache_key_(std::move(cache_key)),
      root_module_key_(std::move(root_module_key)), modules_(std::move(modules)),
      merged_symbols_(std::move(merged_symbols)), import_graph_(std::move(import_graph)),
      c_headers_(std::move(c_headers)), resolutions_(std::move(resolutions)),
      diagnostics_(std::move(diagnostics)), metrics_(metrics) {}

const ModuleArtifact *CompilationSnapshot::findModule(const std::string_view key) const noexcept {
    const auto found = std::lower_bound(
        modules_.begin(), modules_.end(), key,
        [](const ModuleArtifactPtr &module, std::string_view name) { return module->key < name; });
    if (found == modules_.end() || (*found)->key != key)
        return nullptr;
    return found->get();
}

const ModuleResolution *
CompilationSnapshot::findResolution(const std::string_view key) const noexcept {
    const auto found =
        std::lower_bound(resolutions_.begin(), resolutions_.end(), key,
                         [](const ModuleResolution &resolution, std::string_view name) {
                             return resolution.module < name;
                         });
    if (found == resolutions_.end() || found->module != key)
        return nullptr;
    return &*found;
}

bool CompilationSnapshot::hasErrors() const noexcept {
    return std::any_of(diagnostics_.begin(), diagnostics_.end(),
                       [](const ModuleDiagnostic &diagnostic) {
                           return diagnostic.severity == diagnostics::Severity::Error ||
                                  diagnostic.severity == diagnostics::Severity::Bug;
                       });
}

std::string ModuleCache::bucketKey(const CacheKey &cache_key, const std::string_view path) {
    return cache_key.identity() + "\x1f" + std::string(path);
}

std::string ModuleCache::inFlightKey(const CacheKey &cache_key, const SourceRecord &source) {
    return bucketKey(cache_key, source.canonicalPath) + "\x1e" + source.fingerprint.toString();
}

std::shared_future<ModuleArtifactPtr> ModuleCache::readyFuture(ModuleArtifactPtr artifact) const {
    std::promise<ModuleArtifactPtr> promise;
    auto future = promise.get_future().share();
    promise.set_value(std::move(artifact));
    return future;
}

std::shared_future<ModuleArtifactPtr>
ModuleCache::getOrBuild(const CacheKey &cache_key, SourceCatalog::SourcePtr source,
                        ModuleExecutor &executor, std::function<ModuleArtifactPtr()> build) {
    const auto bucket    = bucketKey(cache_key, source->canonicalPath);
    const auto in_flight = inFlightKey(cache_key, *source);
    uint64_t epoch       = 0;
    std::shared_ptr<std::promise<ModuleArtifactPtr>> promise;
    std::shared_future<ModuleArtifactPtr> shared;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto *current = current_fingerprints_.get(source->canonicalPath);
        if (!current || *current != source->fingerprint) {
            current_fingerprints_[source->canonicalPath] = source->fingerprint;
            ++epochs_[source->canonicalPath];
            invalidateLocked(source->canonicalPath);
        }
        if (const auto *cached = artifacts_.get(bucket);
            cached && (*cached)->fingerprint == source->fingerprint) {
            ++metrics_.hits;
            return readyFuture(*cached);
        }
        if (const auto *active = in_flight_.get(in_flight)) {
            ++metrics_.hits;
            return active->future;
        }
        ++metrics_.misses;
        epoch   = epochs_[source->canonicalPath];
        promise = std::make_shared<std::promise<ModuleArtifactPtr>>();
        shared  = promise->get_future().share();
        in_flight_.insert(in_flight, InFlight{shared, epoch});
    }

    (void)executor.submit([this, bucket, in_flight, worker_source = source, epoch,
                           worker_promise = std::move(promise),
                           worker_build   = std::move(build)]() mutable {
        auto artifact = worker_build();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            in_flight_.erase(in_flight);

            if (artifact && epochs_[worker_source->canonicalPath] == epoch &&
                current_fingerprints_[worker_source->canonicalPath] == worker_source->fingerprint) {
                artifacts_[bucket] = artifact;
                metrics_.entries   = artifacts_.size();
            }
        }
        worker_promise->set_value(std::move(artifact));
    });
    return std::shared_future<ModuleArtifactPtr>{shared};
}

void ModuleCache::noteSource(const SourceCatalog::SourcePtr &source) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto *current = current_fingerprints_.get(source->canonicalPath);
    if (current && *current == source->fingerprint)
        return;

    current_fingerprints_[source->canonicalPath] = source->fingerprint;
    ++epochs_[source->canonicalPath];
    invalidateLocked(source->canonicalPath);
}

void ModuleCache::updateDependencies(const ModuleKey &module, std::vector<ModuleKey> dependencies) {
    std::sort(dependencies.begin(), dependencies.end());
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());

    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto *previous = dependencies_.get(module)) {
        for (const auto &dependency : *previous)
            reverse_dependencies_[dependency].erase(module);
    }

    auto &stored = dependencies_[module];
    stored.clear();
    for (auto &dependency : dependencies) {
        stored.insert(dependency);
        reverse_dependencies_[dependency].insert(module);
    }
}

void ModuleCache::invalidate(const std::string_view canonical_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    ++epochs_[std::string(canonical_path)];
    invalidateLocked(std::string(canonical_path));
}

void ModuleCache::invalidateLocked(const ModuleKey &module) {
    std::vector<ModuleKey> pending{module};
    memory::FlatSet<ModuleKey> visited;
    while (!pending.empty()) {
        auto current = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(current))
            continue;

        std::vector<std::string> evict_keys;
        for (const auto item : artifacts_) {
            const auto &[key, artifact] = item;
            if (artifact->key == current)
                evict_keys.push_back(key);
        }
        for (const auto &key : evict_keys) {
            artifacts_.erase(key);
            ++metrics_.invalidated;
        }
        if (const auto *dependents = reverse_dependencies_.get(current)) {
            for (const auto &dependent : *dependents) {
                ++epochs_[dependent];
                pending.push_back(dependent);
            }
        }
    }
    metrics_.entries = artifacts_.size();
}

ModuleCacheMetrics ModuleCache::metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

} // namespace zith::session
