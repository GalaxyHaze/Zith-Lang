#pragma once

#include "memory/result.hpp"

#include <filesystem>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

namespace zith::packages {

struct DependencyDeclaration {
    std::string name;
    std::filesystem::path path;
};

struct PackageManifest {
    std::string name;
    std::string version;
    std::filesystem::path root;
    std::filesystem::path moduleRoot;
    std::vector<DependencyDeclaration> dependencies;
};

[[nodiscard]] memory::Result<PackageManifest>
parseManifest(const toml::table &table, const std::filesystem::path &manifestPath);

} // namespace zith::packages
