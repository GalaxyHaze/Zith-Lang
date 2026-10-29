#pragma once

#include "memory/result.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <toml++/toml.hpp>
#include <vector>

namespace zith::packages {

inline constexpr std::int64_t kLockfileFormatVersion = 1;

struct LockedPackage {
    std::string name;
    std::string version;
    std::filesystem::path path;
    std::filesystem::path moduleRoot;
    std::string manifestSha256;
    std::vector<std::string> dependencies;
};

struct Lockfile {
    std::string projectName;
    std::string projectVersion;
    std::string projectManifestSha256;
    std::vector<std::string> dependencies;
    std::vector<LockedPackage> packages;
};

/// Parse and validate the semantic contents of a TOML lockfile table.
[[nodiscard]] memory::Result<Lockfile> parseLockfile(const toml::table &table);

/// Serialize a valid lockfile as canonical, deterministic TOML.
[[nodiscard]] memory::Result<std::string> serializeLockfile(const Lockfile &lockfile);

} // namespace zith::packages
