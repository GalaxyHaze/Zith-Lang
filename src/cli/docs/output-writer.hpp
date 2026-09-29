#pragma once

#include "cli/docs/doc-model.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace zith::cli::docs {

struct OutputWriteResult {
    bool ok = false;
    std::string message;
};

/// Resolves an explicit output directory or the default project/source docs directory.
[[nodiscard]] std::filesystem::path
resolveDocsOutputDirectory(const std::filesystem::path &inputPath,
                           const std::filesystem::path &explicitOutputPath = {});

/// Writes the complete output set after validating all destinations and collisions.
[[nodiscard]] OutputWriteResult writeDocsOutput(const std::filesystem::path &outputDirectory,
                                                const DocModel &model, bool index, bool force,
                                                std::string_view errorMarkdown = {});

} // namespace zith::cli::docs
