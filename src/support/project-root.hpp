#pragma once

#include <filesystem>
#include <optional>

namespace zith::support {

/// Locate the nearest ancestor of `start` (inclusive) that contains a
/// `ZithProject.toml`.
///
/// The walk stops at a version-control work-tree boundary: a directory that
/// contains a `.git` entry. This keeps a project lookup inside the repository
/// the user is actually working in, so an unrelated `ZithProject.toml` sitting
/// above the checkout is never mistaken for the current project. Returns
/// `std::nullopt` when no manifest is found before the boundary or the
/// filesystem root.
std::optional<std::filesystem::path> findProjectRoot(const std::filesystem::path &start);

} // namespace zith::support
