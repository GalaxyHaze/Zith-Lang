#pragma once

#include "../document_manager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

json runDiagnostics(DocumentManager& docManager, const std::string& uri);
json findDefinition(DocumentManager& docManager, const std::string& uri, int line, int character);
json findHover(DocumentManager& docManager, const std::string& uri, int line, int character);
json getSemanticTokens(DocumentManager& docManager, const std::string& uri);
json executeCompilerCommand(DocumentManager& docManager, const std::string& command, const json& params);
