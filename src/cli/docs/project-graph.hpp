#pragma once

#include "session/frontend-context.hpp"

#include <string>
#include <vector>

namespace zith::cli::docs {

struct GraphModule {
    session::ModuleArtifactPtr artifact;
    bool isDependency = false;
};

struct ProjectGraph {
    std::string entryModule;
    std::vector<GraphModule> modules;
    std::string error;

    [[nodiscard]] bool ok() const noexcept {
        return error.empty();
    }
};

/// Returns the reachable Zith modules in stable key order. Dependency roots and
/// modules outside the workspace are marked separately from project modules.
[[nodiscard]] ProjectGraph buildProjectGraph(const session::CompilationSnapshot &snapshot);

} // namespace zith::cli::docs
