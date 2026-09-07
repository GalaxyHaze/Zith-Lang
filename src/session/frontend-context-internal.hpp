#pragma once

#include "session/frontend-context.hpp"

#include <string>
#include <vector>

namespace zith::session {

/// Internal helpers shared only between the frontend context translation units.
/// These are not part of the public FrontendContext API.
void normalizeRoots(std::vector<std::string> &roots);
void applyConfig(FrontendConfig &config);
[[nodiscard]] std::string joinPath(const std::vector<std::string> &path);
[[nodiscard]] ModuleDiagnostic makeImportDiagnostic(const ModuleArtifact &artifact,
                                                    const ImportRequest &request,
                                                    std::string message);

} // namespace zith::session
