#include "handler_interface.h"
#include "../document_manager.h"
#include <zith/zith.hpp>
#include <zith/parser.h>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

static std::string getTextAtPosition(const std::string& content, int line, int character) {
    std::istringstream stream(content);
    std::string lineStr;
    int currentLine = 0;
    while (std::getline(stream, lineStr)) {
        if (currentLine == line) {
            if (character < (int)lineStr.length()) {
                return lineStr.substr(character);
            }
            return "";
        }
        currentLine++;
    }
    return "";
}

json runDiagnostics(DocumentManager& docManager, const std::string& uri) {
    json diagnostics = json::array();

    auto doc = docManager.getDocument(uri);
    if (!doc) {
        return diagnostics;
    }

    const std::string& content = doc->getContent();
    const std::string& filepath = doc->path_;

    try {
        zith::Arena arena;
        auto tokens = zith::tokenize(arena, content);

        ZithNode* ast = zith_parse_with_source(
            arena.get(),
            content.data(),
            content.size(),
            filepath.c_str(),
            tokens,
            nullptr,
            0
        );

        ZithDiagList* diags = zith_get_parse_diagnostics();
        if (diags && diags->count > 0) {
            for (size_t i = 0; i < diags->count; i++) {
                const auto& d = diags->items[i];
                json diag;
                diag["range"] = json::object();
                diag["range"]["start"] = json::object();
                diag["range"]["start"]["line"] = d.loc.line > 0 ? d.loc.line - 1 : 0;
                diag["range"]["start"]["character"] = d.loc.index;
                diag["range"]["end"] = json::object();
                diag["range"]["end"]["line"] = d.loc.line > 0 ? d.loc.line - 1 : 0;
                diag["range"]["end"]["character"] = d.loc.index + 1;

                diag["severity"] = (d.severity == ZITH_DIAG_ERROR) ? 1 : 2;
                diag["message"] = d.message ? d.message : "Unknown error";
                diag["source"] = "zith";

                diagnostics.push_back(diag);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing " << filepath << ": " << e.what() << std::endl;
    }

    return diagnostics;
}

json findDefinition(DocumentManager& docManager, const std::string& uri, int line, int character) {
    auto doc = docManager.getDocument(uri);
    if (!doc) {
        return json::null();
    }

    const std::string& content = doc->getContent();

    std::istringstream stream(content);
    std::string lineStr;
    int currentLine = 0;
    while (std::getline(stream, lineStr)) {
        if (currentLine == line) {
            break;
        }
        currentLine++;
    }

    json result = json::array();

    return result;
}

json findHover(DocumentManager& docManager, const std::string& uri, int line, int character) {
    auto doc = docManager.getDocument(uri);
    if (!doc) {
        return json::null();
    }

    json result;
    result["contents"] = json::object();
    result["contents"]["kind"] = "markdown";
    result["contents"]["value"] = "Zith";

    return result;
}