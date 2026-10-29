#pragma once

#include "packages/lockfile.hpp"

#include <filesystem>

namespace zith::packages {

[[nodiscard]] memory::Result<Lockfile>
resolveLocalPackages(const std::filesystem::path &projectManifestPath);

} // namespace zith::packages
