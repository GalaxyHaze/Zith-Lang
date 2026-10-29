#include "packages/resolver.hpp"

#include "packages/manifest.hpp"
#include "packages/sha256.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace zith::packages {
namespace {

namespace fs = std::filesystem;

memory::Error resolverError(std::string message) {
    return memory::Error{std::move(message)};
}

std::string pathKey(const fs::path &path) {
    auto value = path.lexically_normal().generic_string();
    while (value.size() > 1U && value.back() == '/')
        value.pop_back();
    return value;
}

memory::Result<fs::path> canonicalDirectory(const fs::path &path, std::string_view description) {
    std::error_code error;
    if (!fs::is_directory(path, error) || error) {
        const std::string detail = error ? ": " + error.message() : std::string{};
        return resolverError(std::string(description) + " is not a directory: " + path.string() +
                             detail);
    }

    auto canonical = fs::weakly_canonical(path, error);
    if (error)
        return resolverError("cannot resolve " + std::string(description) + " '" + path.string() +
                             "': " + error.message());
    return canonical;
}

memory::Result<fs::path> relativePath(const fs::path &path, const fs::path &base,
                                      std::string_view description) {
    auto relative = path.lexically_relative(base);
    if (relative.empty()) {
        if (path == base)
            return fs::path(".");
        return resolverError(std::string(description) +
                             " cannot be represented relative to the project");
    }
    return relative.lexically_normal();
}

struct ManifestDocument {
    PackageManifest manifest;
    std::string sha256;
};

memory::Result<ManifestDocument> loadManifest(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return resolverError("cannot read package manifest '" + path.string() + "'");

    std::string contents{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad())
        return resolverError("failed while reading package manifest '" + path.string() + "'");

#if TOML_EXCEPTIONS
    try {
        const auto table = toml::parse(contents, path.string());
        auto manifest    = parseManifest(table, path);
        if (!manifest)
            return manifest.error();
        return ManifestDocument{std::move(manifest).value(), sha256Hex(contents)};
    } catch (const toml::parse_error &error) {
        return resolverError("invalid TOML in package manifest '" + path.string() +
                             "': " + error.what());
    }
#else
    auto table = toml::parse(contents, path.string());
    if (!table)
        return resolverError("invalid TOML in package manifest '" + path.string() + "'");
    auto manifest = parseManifest(table, path);
    if (!manifest)
        return manifest.error();
    return ManifestDocument{std::move(manifest).value(), sha256Hex(contents)};
#endif
}

memory::Result<Lockfile> loadLockfile(const fs::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return resolverError("cannot read lockfile '" + path.string() + "'");

    std::string contents{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad())
        return resolverError("failed while reading lockfile '" + path.string() + "'");

#if TOML_EXCEPTIONS
    try {
        const auto table = toml::parse(contents, path.string());
        return parseLockfile(table);
    } catch (const toml::parse_error &error) {
        return resolverError("invalid TOML in lockfile '" + path.string() + "': " + error.what());
    }
#else
    auto table = toml::parse(contents, path.string());
    if (!table)
        return resolverError("invalid TOML in lockfile '" + path.string() + "'");
    return parseLockfile(table);
#endif
}

memory::Error staleLockfile(std::string reason) {
    return resolverError("Zith.lock is stale (" + std::move(reason) +
                         "); run 'zithc deps resolve'");
}

enum class VisitState { Visiting, Complete };

struct PackageNode {
    PackageManifest manifest;
    std::string sha256;
    fs::path canonicalModuleRoot;
    LockedPackage locked;
    VisitState state = VisitState::Visiting;
};

class LocalResolver {
public:
    LocalResolver(fs::path projectRoot, std::string projectName, Lockfile &lockfile)
        : projectRoot_(std::move(projectRoot)), projectName_(std::move(projectName)),
          lockfile_(lockfile) {}

    memory::Result<void> visit(const DependencyDeclaration &declaration) {
        auto canonicalRoot = canonicalDirectory(declaration.path, "dependency path");
        if (!canonicalRoot)
            return canonicalRoot.error();

        const std::string rootKey = pathKey(canonicalRoot.value());
        if (const auto existing = nodes_.find(rootKey); existing != nodes_.end()) {
            if (existing->second.manifest.name != declaration.name)
                return resolverError("dependency '" + declaration.name + "' resolves to package '" +
                                     existing->second.manifest.name + "' at '" + rootKey + "'");
            if (existing->second.state == VisitState::Visiting)
                return cycleError(declaration.name);
            return {};
        }

        const fs::path manifestPath = canonicalRoot.value() / "ZithProject.toml";
        auto document               = loadManifest(manifestPath);
        if (!document)
            return document.error();

        auto manifest = std::move(document).value();
        if (manifest.manifest.name != declaration.name)
            return resolverError("dependency name '" + declaration.name +
                                 "' does not match package name '" + manifest.manifest.name +
                                 "' in '" + manifestPath.string() + "'");
        if (manifest.manifest.name == "std")
            return resolverError("std is bundled with the compiler and cannot be a package");
        if (manifest.manifest.name == projectName_)
            return resolverError("dependency package name conflicts with the project: " +
                                 projectName_);
        if (const auto existing = rootsByName_.find(manifest.manifest.name);
            existing != rootsByName_.end() && existing->second != rootKey)
            return resolverError("package '" + manifest.manifest.name +
                                 "' resolves to multiple paths: '" + existing->second + "' and '" +
                                 rootKey + "'");

        auto canonicalModuleRoot =
            canonicalDirectory(manifest.manifest.moduleRoot, "package module root");
        if (!canonicalModuleRoot)
            return canonicalModuleRoot.error();
        const std::string moduleKey = pathKey(canonicalModuleRoot.value());
        if (const auto existing = moduleOwners_.find(moduleKey);
            existing != moduleOwners_.end() && existing->second != manifest.manifest.name)
            return resolverError("packages '" + existing->second + "' and '" +
                                 manifest.manifest.name + "' resolve to the same module root: '" +
                                 moduleKey + "'");

        auto projectRelativeRoot =
            relativePath(canonicalRoot.value(), projectRoot_, "package path");
        if (!projectRelativeRoot)
            return projectRelativeRoot.error();
        auto packageRelativeModuleRoot =
            relativePath(manifest.manifest.moduleRoot, manifest.manifest.root, "module root");
        if (!packageRelativeModuleRoot)
            return packageRelativeModuleRoot.error();

        auto dependencies = manifest.manifest.dependencies;
        std::sort(dependencies.begin(), dependencies.end(),
                  [](const DependencyDeclaration &left, const DependencyDeclaration &right) {
                      return left.name < right.name;
                  });

        LockedPackage locked{manifest.manifest.name,
                             manifest.manifest.version,
                             std::move(projectRelativeRoot).value(),
                             std::move(packageRelativeModuleRoot).value(),
                             manifest.sha256,
                             {}};
        locked.dependencies.reserve(dependencies.size());
        for (const auto &dependency : dependencies)
            locked.dependencies.push_back(dependency.name);

        PackageNode node{std::move(manifest.manifest), std::move(manifest.sha256),
                         std::move(canonicalModuleRoot).value(), std::move(locked),
                         VisitState::Visiting};
        const auto [nodePosition, inserted] = nodes_.emplace(rootKey, std::move(node));
        if (!inserted)
            return resolverError("duplicate package path while resolving '" + rootKey + "'");
        rootsByName_.emplace(nodePosition->second.manifest.name, rootKey);
        moduleOwners_.emplace(moduleKey, nodePosition->second.manifest.name);
        stack_.push_back(nodePosition->second.manifest.name);

        for (const auto &dependency : dependencies) {
            if (auto result = visit(dependency); !result)
                return result.error();
        }

        stack_.pop_back();
        auto completedNode = nodes_.find(rootKey);
        if (completedNode == nodes_.end())
            return resolverError("package disappeared while resolving '" + rootKey + "'");
        completedNode->second.state = VisitState::Complete;
        lockfile_.packages.push_back(std::move(completedNode->second.locked));
        return {};
    }

private:
    memory::Error cycleError(std::string_view name) const {
        std::ostringstream message;
        message << "local package dependency cycle: ";
        const auto cycleStart = std::find(stack_.begin(), stack_.end(), name);
        bool first            = true;
        for (auto current = cycleStart; current != stack_.end(); ++current) {
            if (!first)
                message << " -> ";
            message << *current;
            first = false;
        }
        if (!first)
            message << " -> ";
        message << name;
        return resolverError(message.str());
    }

    fs::path projectRoot_;
    std::string projectName_;
    Lockfile &lockfile_;
    std::unordered_map<std::string, PackageNode> nodes_;
    std::unordered_map<std::string, std::string> rootsByName_;
    std::unordered_map<std::string, std::string> moduleOwners_;
    std::vector<std::string> stack_;
};

} // namespace

memory::Result<Lockfile> resolveLocalPackages(const fs::path &projectManifestPath) {
    std::error_code error;
    const auto absoluteManifest = fs::absolute(projectManifestPath, error);
    if (error)
        return resolverError("cannot resolve project manifest path '" +
                             projectManifestPath.string() + "': " + error.message());

    const auto canonicalManifest = fs::weakly_canonical(absoluteManifest, error);
    if (error)
        return resolverError("cannot resolve project manifest '" + absoluteManifest.string() +
                             "': " + error.message());
    if (!fs::is_regular_file(canonicalManifest, error) || error)
        return resolverError("project manifest is not a regular file: '" +
                             canonicalManifest.string() + "'");

    const fs::path projectRoot = canonicalManifest.parent_path();
    auto document              = loadManifest(canonicalManifest);
    if (!document)
        return document.error();

    auto projectManifest = std::move(document).value();
    Lockfile lockfile{projectManifest.manifest.name,
                      projectManifest.manifest.version,
                      projectManifest.sha256,
                      {},
                      {}};

    auto dependencies = projectManifest.manifest.dependencies;
    std::sort(dependencies.begin(), dependencies.end(),
              [](const DependencyDeclaration &left, const DependencyDeclaration &right) {
                  return left.name < right.name;
              });
    lockfile.dependencies.reserve(dependencies.size());
    for (const auto &dependency : dependencies)
        lockfile.dependencies.push_back(dependency.name);

    LocalResolver resolver(projectRoot, lockfile.projectName, lockfile);
    for (const auto &dependency : dependencies) {
        if (auto result = resolver.visit(dependency); !result)
            return result.error();
    }

    std::sort(lockfile.packages.begin(), lockfile.packages.end(),
              [](const LockedPackage &left, const LockedPackage &right) {
                  return left.name < right.name;
              });
    if (auto serialized = serializeLockfile(lockfile); !serialized)
        return serialized.error();
    return lockfile;
}

memory::Result<void> validateLocalLockfile(const fs::path &projectManifestPath) {
    std::error_code error;
    const auto absoluteManifest = fs::absolute(projectManifestPath, error);
    if (error)
        return resolverError("cannot resolve project manifest path '" +
                             projectManifestPath.string() + "': " + error.message());

    const auto canonicalManifest = fs::weakly_canonical(absoluteManifest, error);
    if (error)
        return resolverError("cannot resolve project manifest '" + absoluteManifest.string() +
                             "': " + error.message());
    if (!fs::is_regular_file(canonicalManifest, error) || error)
        return resolverError("project manifest is not a regular file: '" +
                             canonicalManifest.string() + "'");

    auto projectDocument = loadManifest(canonicalManifest);
    if (!projectDocument)
        return projectDocument.error();

    const auto projectRoot = canonicalManifest.parent_path();
    auto lockfile          = loadLockfile(projectRoot / "Zith.lock");
    if (!lockfile)
        return resolverError(lockfile.error().msg + "; run 'zithc deps resolve'");

    const auto &project = projectDocument.value().manifest;
    const auto &locked  = lockfile.value();
    if (locked.projectName != project.name || locked.projectVersion != project.version ||
        locked.projectManifestSha256 != projectDocument.value().sha256)
        return staleLockfile("project identity or manifest fingerprint changed");

    std::unordered_map<std::string, const LockedPackage *> packagesByName;
    for (const auto &package : locked.packages)
        packagesByName.emplace(package.name, &package);

    const auto dependencyNames = [](const auto &dependencies) {
        std::vector<std::string> names;
        names.reserve(dependencies.size());
        for (const auto &dependency : dependencies)
            names.push_back(dependency.name);
        std::sort(names.begin(), names.end());
        return names;
    };
    auto lockedProjectDependencies = locked.dependencies;
    std::sort(lockedProjectDependencies.begin(), lockedProjectDependencies.end());
    if (dependencyNames(project.dependencies) != lockedProjectDependencies)
        return staleLockfile("project dependencies changed");

    const auto verifyDependencyPath =
        [&](const DependencyDeclaration &dependency) -> memory::Result<void> {
        const auto package = packagesByName.find(dependency.name);
        if (package == packagesByName.end())
            return staleLockfile("dependency '" + dependency.name + "' is not locked");
        auto declaredRoot = canonicalDirectory(dependency.path, "dependency path");
        if (!declaredRoot)
            return staleLockfile(declaredRoot.error().msg);
        auto lockedRoot = canonicalDirectory(projectRoot / package->second->path, "locked package");
        if (!lockedRoot)
            return staleLockfile(lockedRoot.error().msg);
        if (declaredRoot.value() != lockedRoot.value())
            return staleLockfile("dependency path changed for '" + dependency.name + "'");
        return {};
    };

    for (const auto &dependency : project.dependencies) {
        if (auto result = verifyDependencyPath(dependency); !result)
            return result.error();
    }

    for (const auto &package : locked.packages) {
        const auto packageRoot = projectRoot / package.path;
        auto document          = loadManifest(packageRoot / "ZithProject.toml");
        if (!document)
            return staleLockfile(document.error().msg);

        const auto &manifest = document.value().manifest;
        auto moduleRoot = relativePath(manifest.moduleRoot, manifest.root, "package module root");
        if (!moduleRoot)
            return staleLockfile(moduleRoot.error().msg);
        if (manifest.name != package.name || manifest.version != package.version ||
            document.value().sha256 != package.manifestSha256 ||
            moduleRoot.value() != package.moduleRoot)
            return staleLockfile("package identity or manifest fingerprint changed for '" +
                                 package.name + "'");

        auto lockedDependencies = package.dependencies;
        std::sort(lockedDependencies.begin(), lockedDependencies.end());
        if (dependencyNames(manifest.dependencies) != lockedDependencies)
            return staleLockfile("dependencies changed for package '" + package.name + "'");
        for (const auto &dependency : manifest.dependencies) {
            if (auto result = verifyDependencyPath(dependency); !result)
                return result.error();
        }
    }

    std::unordered_set<std::string> reachablePackages;
    std::vector<std::string> pending = locked.dependencies;
    while (!pending.empty()) {
        auto name = std::move(pending.back());
        pending.pop_back();
        if (!reachablePackages.insert(name).second)
            continue;
        const auto package = packagesByName.find(name);
        if (package == packagesByName.end())
            return staleLockfile("dependency '" + name + "' is not locked");
        pending.insert(pending.end(), package->second->dependencies.begin(),
                       package->second->dependencies.end());
    }
    if (reachablePackages.size() != locked.packages.size())
        return staleLockfile("lockfile contains packages outside the dependency graph");

    return {};
}

} // namespace zith::packages
