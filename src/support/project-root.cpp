#include "support/project-root.hpp"

#include <system_error>

namespace zith::support {

std::optional<std::filesystem::path> findProjectRoot(const std::filesystem::path &start) {
    namespace fs = std::filesystem;

    std::error_code error;
    auto current = fs::weakly_canonical(start, error);
    if (error)
        current = fs::absolute(start, error);
    if (error)
        return std::nullopt;

    if (!fs::is_directory(current, error) || error)
        current = current.parent_path();

    while (!current.empty()) {
        if (fs::exists(current / "ZithProject.toml", error) && !error)
            return current;

        // Do not walk past the version-control work-tree root: a manifest above
        // the checkout belongs to a different project and must not be adopted.
        if (fs::exists(current / ".git", error) && !error)
            break;

        const auto parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }

    return std::nullopt;
}

} // namespace zith::support
