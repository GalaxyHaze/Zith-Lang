#include "packages/manifest.hpp"

namespace zith::packages {

namespace fs = std::filesystem;

namespace {

memory::Error manifestError(std::string message) {
    return memory::Error{std::move(message)};
}

} // namespace

memory::Result<PackageManifest> parseManifest(const toml::table &table,
                                              const fs::path &manifestPath) {
    const auto *projectNode = table.get("project");
    if (projectNode == nullptr)
        return manifestError("missing [project] table");
    const auto *project = projectNode->as_table();
    if (project == nullptr)
        return manifestError("[project] must be a table");

    const auto name = (*project)["name"].value<std::string>();
    if (!name || name->empty())
        return manifestError("[project].name must be a non-empty string");

    const auto version = (*project)["version"].value<std::string>();
    if (!version || version->empty())
        return manifestError("[project].version must be a non-empty string");

    const fs::path packageRoot = manifestPath.parent_path().lexically_normal();
    fs::path moduleRootPath    = "src";
    if (const auto *packageNode = table.get("package")) {
        const auto *package = packageNode->as_table();
        if (package == nullptr)
            return manifestError("[package] must be a table");
        if (const auto *moduleRootNode = package->get("module_root")) {
            const auto value = moduleRootNode->value<std::string>();
            if (!value || value->empty())
                return manifestError("[package].module_root must be a non-empty path string");
            moduleRootPath = *value;
        }
    }
    if (moduleRootPath.has_root_name() || moduleRootPath.has_root_directory())
        return manifestError("[package].module_root must be a relative path");

    PackageManifest result;
    result.name       = *name;
    result.version    = *version;
    result.root       = packageRoot;
    result.moduleRoot = (packageRoot / moduleRootPath).lexically_normal();

    if (const auto *dependenciesNode = table.get("dependencies")) {
        const auto *dependencies = dependenciesNode->as_table();
        if (dependencies == nullptr)
            return manifestError("[dependencies] must be a table");
        for (const auto &[key, value] : *dependencies) {
            const std::string dependencyName(key.str());
            if (dependencyName.empty())
                return manifestError("[dependencies] package names must not be empty");
            if (dependencyName == "std")
                return manifestError(
                    "std is bundled with the compiler and cannot be a package dependency");

            const auto dependencyPath = value.value<std::string>();
            if (!dependencyPath || dependencyPath->empty())
                return manifestError("[dependencies]." + dependencyName +
                                     " must be a non-empty local path string");

            const fs::path relativePath(*dependencyPath);
            if (relativePath.has_root_name() || relativePath.has_root_directory())
                return manifestError("[dependencies]." + dependencyName +
                                     " must be a relative local path");

            result.dependencies.push_back(
                {dependencyName, (packageRoot / relativePath).lexically_normal()});
        }
    }

    return result;
}

} // namespace zith::packages
