#include "project-graph.hpp"

#include <filesystem>
#include <map>
#include <set>
#include <string_view>

namespace zith::cli::docs {

namespace {

bool isWithinPath(const std::string_view candidate, const std::string_view root) {
    if (root.empty())
        return false;

    const std::filesystem::path candidatePath = std::filesystem::path(candidate).lexically_normal();
    const std::filesystem::path rootPath      = std::filesystem::path(root).lexically_normal();
    const auto relativePath                   = candidatePath.lexically_relative(rootPath);
    if (relativePath.empty())
        return candidatePath == rootPath;

    const auto first = relativePath.begin();
    return first == relativePath.end() || *first != "..";
}

bool isDependencyModule(const session::ModuleArtifact &module, const session::CacheKey &cacheKey) {
    const std::string_view path = module.source ? module.source->canonicalPath : module.key;
    for (const auto &root : cacheKey.stdlibRoots)
        if (isWithinPath(path, root))
            return true;

    for (const auto &root : cacheKey.includeRoots)
        if (!isWithinPath(root, cacheKey.workspaceRoot) && isWithinPath(path, root))
            return true;

    return !cacheKey.workspaceRoot.empty() && !isWithinPath(path, cacheKey.workspaceRoot);
}

} // namespace

ProjectGraph buildProjectGraph(const session::CompilationSnapshot &snapshot) {
    ProjectGraph result;
    result.entryModule = snapshot.rootModuleKey();

    std::map<session::ModuleKey, session::ModuleArtifactPtr> modulesByKey;
    for (const auto &module : snapshot.modules())
        if (module)
            modulesByKey.emplace(module->key, module);

    if (!modulesByKey.contains(result.entryModule)) {
        result.error = "snapshot entry module is missing from the module list";
        return result;
    }

    std::set<session::ModuleKey> reachable;
    std::set<session::ModuleKey> pending{result.entryModule};
    while (!pending.empty()) {
        const auto current = *pending.begin();
        pending.erase(pending.begin());
        if (!reachable.insert(current).second)
            continue;

        for (const auto &edge : snapshot.importGraph()) {
            if (edge.importer != current || edge.targetKind != session::ImportTargetKind::Zith)
                continue;
            for (const auto &target : edge.targets)
                if (modulesByKey.contains(target))
                    pending.insert(target);
        }
    }

    result.modules.reserve(reachable.size());
    for (const auto &key : reachable) {
        const auto found = modulesByKey.find(key);
        if (found == modulesByKey.end())
            continue;
        result.modules.push_back(
            {found->second, isDependencyModule(*found->second, snapshot.cacheKey())});
    }
    return result;
}

} // namespace zith::cli::docs
