#pragma once

#include "document_manager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

json runDiagnostics(DocumentManager& docManager, const std::string& uri);
json findDefinition(DocumentManager& docManager, const std::string& uri, int line, int character);
json findHover(DocumentManager& docManager, const std::string& uri, int line, int character);