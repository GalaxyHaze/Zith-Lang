#include "packages/lockfile.hpp"

#include <algorithm>
#include <queue>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace zith::packages {

namespace fs = std::filesystem;

namespace {

memory::Error lockfileError(std::string message) {
    return memory::Error{std::move(message)};
}

bool isSha256(std::string_view value) {
    if (value.size() != 64U)
        return false;
    return std::all_of(value.begin(), value.end(), [](const char rawCharacter) {
        const auto character = static_cast<unsigned char>(rawCharacter);
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

std::string pathKey(const fs::path &path) {
    auto value = path.lexically_normal().generic_string();
    while (value.size() > 1U && value.back() == '/')
        value.pop_back();
    return value;
}

bool isNormalizedRelativePathString(std::string_view value) {
    if (value.empty() || value.find('\\') != std::string_view::npos ||
        value.find(':') != std::string_view::npos)
        return false;
    const fs::path path{std::string(value)};
    return !path.has_root_name() && !path.has_root_directory() && value == pathKey(path);
}

bool isNormalizedRelativePath(const fs::path &path) {
    return isNormalizedRelativePathString(path.generic_string());
}

memory::Result<std::string> requiredString(const toml::table &table, std::string_view key,
                                           std::string_view field) {
    const auto value = table[key].value<std::string>();
    if (!value || value->empty())
        return lockfileError(std::string(field) + " must be a non-empty string");
    return *value;
}

memory::Result<std::vector<std::string>>
readStringArray(const toml::table &table, std::string_view key, std::string_view field) {
    const auto *node = table.get(key);
    if (node == nullptr)
        return lockfileError(std::string(field) + " is required");
    const auto *array = node->as_array();
    if (array == nullptr)
        return lockfileError(std::string(field) + " must be an array of strings");

    std::vector<std::string> values;
    values.reserve(array->size());
    for (const auto &item : *array) {
        const auto value = item.value<std::string>();
        if (!value || value->empty())
            return lockfileError(std::string(field) + " entries must be non-empty strings");
        values.push_back(*value);
    }
    return values;
}

memory::Result<void> validateLockfile(const Lockfile &lockfile) {
    if (lockfile.projectName.empty() || lockfile.projectVersion.empty())
        return lockfileError("lockfile project name and version must be non-empty");
    if (!isSha256(lockfile.projectManifestSha256))
        return lockfileError("project manifest fingerprint must be a lowercase SHA-256 digest");

    std::unordered_set<std::string> packageNames;
    std::unordered_set<std::string> packagePaths;
    std::unordered_set<std::string> modulePaths;
    for (const auto &package : lockfile.packages) {
        if (package.name.empty() || package.version.empty())
            return lockfileError("locked package name and version must be non-empty");
        if (package.name == "std")
            return lockfileError("std cannot be a locked package dependency");
        if (package.name == lockfile.projectName)
            return lockfileError("project package name conflicts with a locked dependency: " +
                                 package.name);
        if (!isSha256(package.manifestSha256))
            return lockfileError("package manifest fingerprint must be a lowercase SHA-256 digest");
        if (!isNormalizedRelativePath(package.path))
            return lockfileError("locked package path must be normalized and project-relative");
        if (!isNormalizedRelativePath(package.moduleRoot))
            return lockfileError("locked module root must be normalized and package-relative");
        if (!packageNames.insert(package.name).second)
            return lockfileError("duplicate locked package name: " + package.name);
        if (!packagePaths.insert(pathKey(package.path)).second)
            return lockfileError("duplicate locked package path: " + package.path.generic_string());
        const auto modulePath = package.path / package.moduleRoot;
        if (!modulePaths.insert(pathKey(modulePath)).second)
            return lockfileError("duplicate locked module root: " + modulePath.generic_string());
    }

    const auto validateEdges = [&packageNames](const std::vector<std::string> &edges,
                                               std::string_view owner) -> memory::Result<void> {
        std::unordered_set<std::string> uniqueEdges;
        for (const auto &dependency : edges) {
            if (dependency.empty() || !uniqueEdges.insert(dependency).second)
                return lockfileError(std::string(owner) + " has an empty or duplicate dependency");
            if (!packageNames.contains(dependency))
                return lockfileError(std::string(owner) +
                                     " refers to an unlocked package: " + dependency);
        }
        return {};
    };

    if (auto result = validateEdges(lockfile.dependencies, "project"); !result)
        return result.error();
    for (const auto &package : lockfile.packages) {
        if (auto result = validateEdges(package.dependencies, package.name); !result)
            return result.error();
    }

    std::unordered_map<std::string, size_t> remainingDependencies;
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    for (const auto &package : lockfile.packages) {
        remainingDependencies.emplace(package.name, package.dependencies.size());
        for (const auto &dependency : package.dependencies)
            dependents[dependency].push_back(package.name);
    }
    std::queue<std::string> ready;
    for (const auto &[name, count] : remainingDependencies) {
        if (count == 0U)
            ready.push(name);
    }
    size_t visitedCount = 0;
    while (!ready.empty()) {
        const auto name = std::move(ready.front());
        ready.pop();
        ++visitedCount;
        for (const auto &dependent : dependents[name]) {
            auto &count = remainingDependencies.at(dependent);
            --count;
            if (count == 0U)
                ready.push(dependent);
        }
    }
    if (visitedCount != lockfile.packages.size())
        return lockfileError("locked package graph contains a dependency cycle");
    return {};
}

} // namespace

memory::Result<Lockfile> parseLockfile(const toml::table &table) {
    const auto *lockNode = table.get("lock");
    if (lockNode == nullptr || lockNode->as_table() == nullptr)
        return lockfileError("missing [lock] table");
    const auto &lockTable = *lockNode->as_table();
    const auto version    = lockTable["version"].value<std::int64_t>();
    if (!version || *version != kLockfileFormatVersion)
        return lockfileError("unsupported or missing lockfile format version");

    const auto *projectNode = table.get("project");
    if (projectNode == nullptr || projectNode->as_table() == nullptr)
        return lockfileError("missing [project] lockfile table");
    const auto &projectTable = *projectNode->as_table();

    auto projectName = requiredString(projectTable, "name", "[project].name");
    if (!projectName)
        return projectName.error();
    auto projectVersion = requiredString(projectTable, "version", "[project].version");
    if (!projectVersion)
        return projectVersion.error();
    auto projectFingerprint =
        requiredString(projectTable, "manifest_sha256", "[project].manifest_sha256");
    if (!projectFingerprint)
        return projectFingerprint.error();
    auto directDependencies =
        readStringArray(projectTable, "dependencies", "[project].dependencies");
    if (!directDependencies)
        return directDependencies.error();

    Lockfile result;
    result.projectName           = std::move(projectName).value();
    result.projectVersion        = std::move(projectVersion).value();
    result.projectManifestSha256 = std::move(projectFingerprint).value();
    result.dependencies          = std::move(directDependencies).value();

    const auto *packagesNode = table.get("package");
    if (packagesNode == nullptr || packagesNode->as_array() == nullptr)
        return lockfileError("package records must be an array");
    const auto *packages = packagesNode->as_array();
    result.packages.reserve(packages->size());
    for (const auto &node : *packages) {
        const auto *package = node.as_table();
        if (package == nullptr)
            return lockfileError("[[package]] entries must be tables");

        auto name = requiredString(*package, "name", "[[package]].name");
        if (!name)
            return name.error();
        auto packageVersion = requiredString(*package, "version", "[[package]].version");
        if (!packageVersion)
            return packageVersion.error();
        auto path = requiredString(*package, "path", "[[package]].path");
        if (!path)
            return path.error();
        if (!isNormalizedRelativePathString(path.value()))
            return lockfileError("[[package]].path must be normalized and project-relative");
        auto moduleRoot = requiredString(*package, "module_root", "[[package]].module_root");
        if (!moduleRoot)
            return moduleRoot.error();
        if (!isNormalizedRelativePathString(moduleRoot.value()))
            return lockfileError("[[package]].module_root must be normalized and package-relative");
        auto fingerprint =
            requiredString(*package, "manifest_sha256", "[[package]].manifest_sha256");
        if (!fingerprint)
            return fingerprint.error();
        auto dependencies = readStringArray(*package, "dependencies", "[[package]].dependencies");
        if (!dependencies)
            return dependencies.error();

        result.packages.push_back(
            {std::move(name).value(), std::move(packageVersion).value(),
             fs::path(std::move(path).value()), fs::path(std::move(moduleRoot).value()),
             std::move(fingerprint).value(), std::move(dependencies).value()});
    }

    if (auto validation = validateLockfile(result); !validation)
        return validation.error();
    return result;
}

memory::Result<std::string> serializeLockfile(const Lockfile &lockfile) {
    if (auto validation = validateLockfile(lockfile); !validation)
        return validation.error();

    const auto makeDependencies = [](std::vector<std::string> values) {
        std::sort(values.begin(), values.end());
        toml::array array;
        for (auto &value : values)
            array.push_back(std::move(value));
        return array;
    };

    toml::table lockTable;
    lockTable.insert("version", kLockfileFormatVersion);

    toml::table projectTable;
    projectTable.insert("name", lockfile.projectName);
    projectTable.insert("version", lockfile.projectVersion);
    projectTable.insert("manifest_sha256", lockfile.projectManifestSha256);
    projectTable.insert("dependencies", makeDependencies(lockfile.dependencies));

    auto packages = lockfile.packages;
    std::sort(packages.begin(), packages.end(),
              [](const LockedPackage &left, const LockedPackage &right) {
                  return left.name < right.name;
              });
    toml::array packageArray;
    for (auto &package : packages) {
        toml::table packageTable;
        packageTable.insert("name", package.name);
        packageTable.insert("version", package.version);
        packageTable.insert("path", package.path.generic_string());
        packageTable.insert("module_root", package.moduleRoot.generic_string());
        packageTable.insert("manifest_sha256", package.manifestSha256);
        packageTable.insert("dependencies", makeDependencies(std::move(package.dependencies)));
        packageArray.push_back(std::move(packageTable));
    }

    toml::table outputTable;
    outputTable.insert("lock", std::move(lockTable));
    outputTable.insert("project", std::move(projectTable));
    outputTable.insert("package", std::move(packageArray));

    std::ostringstream output;
    output << toml::toml_formatter{outputTable} << '\n';
    return output.str();
}

} // namespace zith::packages
