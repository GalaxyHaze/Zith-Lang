#include "handler_interface.h"
#include "../document_manager.h"
#include <zith/zith.hpp>
#include <zith/parser.h>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

static size_t byteOffsetToColumn(const std::string& content, size_t byteOffset) {
    if (byteOffset >= content.size()) byteOffset = content.size() > 0 ? content.size() - 1 : 0;
    size_t lineStart = content.rfind('\n', byteOffset);
    if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
    size_t col = 0;
    for (size_t i = lineStart; i < byteOffset; ++col) {
        unsigned char c = content[i];
        if ((c & 0xF8) == 0xF0) i += 4;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else i += 1;
    }
    return col;
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

        zith_parse_with_source(
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
                int line = d.loc.line > 0 ? static_cast<int>(d.loc.line) - 1 : 0;
                int col = static_cast<int>(byteOffsetToColumn(content, d.loc.index));

                diag["range"] = json::object();
                diag["range"]["start"] = json::object();
                diag["range"]["start"]["line"] = line;
                diag["range"]["start"]["character"] = col;
                diag["range"]["end"] = json::object();
                diag["range"]["end"]["line"] = line;

                size_t tokenLen = 1;
                if (tokens.data && tokens.len > 0) {
                    for (size_t ti = 0; ti < tokens.len; ++ti) {
                        if (tokens.data[ti].loc.index == d.loc.index) {
                            tokenLen = tokens.data[ti].lexeme.len;
                            if (tokenLen == 0) tokenLen = 1;
                            break;
                        }
                    }
                }
                size_t endCol = byteOffsetToColumn(content, d.loc.index + tokenLen);
                diag["range"]["end"]["character"] = static_cast<int>(endCol);

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

json findDefinition(DocumentManager& docManager, const std::string& uri, int, int) {
    auto doc = docManager.getDocument(uri);
    if (!doc) {
        return json();
    }

    json result = json::array();
    return result;
}

json findHover(DocumentManager& docManager, const std::string& uri, int, int) {
    auto doc = docManager.getDocument(uri);
    if (!doc) {
        return json();
    }

    json result;
    result["contents"] = json::object();
    result["contents"]["kind"] = "markdown";
    result["contents"]["value"] = "Zith";
    return result;
}
