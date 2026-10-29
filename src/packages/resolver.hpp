#pragma once

#include "packages/lockfile.hpp"

#include <filesystem>

namespace zith::packages {

[[nodiscard]] memory::Result<Lockfile>
resolveLocalPackages(const std::filesystem::path &projectManifestPath);

/// Validate the project's lockfile against its current manifest graph without writing files.
[[nodiscard]] memory::Result<void>
validateLocalLockfile(const std::filesystem::path &projectManifestPath);

} // namespace zith::packages
