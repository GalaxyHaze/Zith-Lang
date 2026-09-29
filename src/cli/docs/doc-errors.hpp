#pragma once

#include "cli/docs/doc-model.hpp"
#include "cli/docs/project-graph.hpp"
#include "session/compilation-session.hpp"

#include <string>
#include <vector>

namespace zith::cli::docs {

struct DocError {
    std::string file;
    std::string module;
    std::string section;
    std::string message;
    uint32_t start = 0;
    uint32_t end   = 0;
};

/// Collects compiler and documentation-generation errors in stable source order.
[[nodiscard]] std::vector<DocError> collectDocErrors(session::CompilationSession &session,
                                                     const ProjectGraph &graph,
                                                     const DocModel &model);

/// Renders grouped errors as a standalone Markdown section.
[[nodiscard]] std::string renderDocErrors(const std::vector<DocError> &errors);

} // namespace zith::cli::docs
