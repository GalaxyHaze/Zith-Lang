#pragma once

#include "cli/docs/doc-model.hpp"

#include <string>

namespace zith::cli::docs {

/// Renders a deterministic Markdown document from a documentation model.
[[nodiscard]] std::string renderMarkdown(const DocModel &model);

} // namespace zith::cli::docs
