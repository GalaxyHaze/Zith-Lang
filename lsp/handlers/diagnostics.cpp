#include "handler_interface.h"
#include "../document_manager.h"
#include <zith/zith.hpp>
#include <zith/parser.h>
#include <zith/diagnostics.h>
#include "diagnostics/diagnostics.hpp"
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

static int lspSeverityFromV2(const std::string& level) {
    // LSP severity: 1=Error, 2=Warning, 3=Information, 4=Hint
    if (level == "error" || level == "fatal error" || level == "internal compiler error") return 1;
    if (level == "warning") return 2;
    if (level == "note") return 3;
    return 4; // help, hint
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

        // Try v2 path first: get DiagManager for full features
        DiagManager* dm = zith_get_parse_diag_manager();
        if (dm && dm->bag().total_count() > 0) {
            // Use v2 JSON emitter for full feature support
            zith::diag::JsonEmitter emitter(&dm->source_map());
            std::string v2_json = emitter.emit_to_string(dm->bag());

            if (!v2_json.empty() && v2_json != "[]\n") {
                // Parse v2 JSON and convert to LSP format
                auto v2_diags = json::parse(v2_json);
                for (const auto& d : v2_diags) {
                    json diag;

                    // Range from v2 line/column (1-based) to LSP (0-based)
                    int line = d.value("line", 1);
                    int col = d.value("column", 1);
                    int endLine = d.value("end_line", line);
                    int endCol = d.value("end_column", col);

                    // If no end span, calculate from token length
                    if (endLine == line && endCol == col) {
                        size_t byteOffset = 0;
                        // Approximate: use column as byte offset for now
                        // A more accurate approach would track token lengths
                        byteOffset = static_cast<size_t>(col - 1);
                        size_t tokenLen = 1;
                        if (tokens.data && tokens.len > 0) {
                            for (size_t ti = 0; ti < tokens.len; ++ti) {
                                if (tokens.data[ti].loc.line == static_cast<size_t>(line) &&
                                    tokens.data[ti].loc.index >= byteOffset &&
                                    tokens.data[ti].loc.index < byteOffset + 10) {
                                    tokenLen = tokens.data[ti].lexeme.len;
                                    if (tokenLen == 0) tokenLen = 1;
                                    byteOffset = tokens.data[ti].loc.index;
                                    break;
                                }
                            }
                        }
                        endCol = static_cast<int>(byteOffsetToColumn(content, byteOffset + tokenLen));
                        col = static_cast<int>(byteOffsetToColumn(content, byteOffset));
                        line = line > 0 ? line - 1 : 0;
                    } else {
                        line = line > 0 ? line - 1 : 0;
                        endLine = endLine > 0 ? endLine - 1 : line;
                        col = col > 0 ? col - 1 : 0;
                        endCol = endCol > 0 ? endCol - 1 : col;
                    }

                    diag["range"] = json::object();
                    diag["range"]["start"] = json::object();
                    diag["range"]["start"]["line"] = line;
                    diag["range"]["start"]["character"] = col;
                    diag["range"]["end"] = json::object();
                    diag["range"]["end"]["line"] = endLine;
                    diag["range"]["end"]["character"] = endCol;

                    // Severity
                    std::string severity = d.value("severity", "error");
                    diag["severity"] = lspSeverityFromV2(severity);

                    // Message
                    diag["message"] = d.value("message", "Unknown error");

                    // Source
                    diag["source"] = "zith";

                    // Error code (v2 feature)
                    if (d.contains("code")) {
                        diag["code"] = d["code"];
                    }

                    // Related information from secondary spans (v2 feature)
                    if (d.contains("suggestions") && d["suggestions"].is_array() && !d["suggestions"].empty()) {
                        json relatedInfo = json::array();
                        for (const auto& sug : d["suggestions"]) {
                            json related;
                            related["message"] = sug.value("label", "");
                            if (sug.contains("span")) {
                                int sugLine = sug["span"]["start"].value("line", 1);
                                int sugCol = sug["span"]["start"].value("column", 1);
                                related["location"] = json::object();
                                related["location"]["uri"] = uri;
                                related["location"]["range"] = json::object();
                                related["location"]["range"]["start"] = json::object();
                                related["location"]["range"]["start"]["line"] = sugLine > 0 ? sugLine - 1 : 0;
                                related["location"]["range"]["start"]["character"] = sugCol > 0 ? sugCol - 1 : 0;
                                related["location"]["range"]["end"] = json::object();
                                related["location"]["range"]["end"]["line"] = sugLine > 0 ? sugLine - 1 : 0;
                                related["location"]["range"]["end"]["character"] = sugCol > 0 ? sugCol - 1 : 0;
                            }
                            relatedInfo.push_back(related);
                        }
                        diag["relatedInformation"] = relatedInfo;
                    }

                    // Children as additional related info (notes, helps)
                    if (d.contains("children") && d["children"].is_array() && !d["children"].empty()) {
                        // Append child messages to the main message for visibility
                        std::string fullMsg = diag["message"];
                        for (const auto& child : d["children"]) {
                            std::string level = child.value("level", "");
                            std::string msg = child.value("message", "");
                            if (!msg.empty()) {
                                fullMsg += "\n" + level + ": " + msg;
                            }
                        }
                        diag["message"] = fullMsg;
                    }

                    // Code description with help text (v2 feature)
                    if (d.contains("children") && d["children"].is_array()) {
                        for (const auto& child : d["children"]) {
                            std::string level = child.value("level", "");
                            if (level == "help") {
                                diag["codeDescription"] = json::object();
                                diag["codeDescription"]["message"] = child.value("message", "");
                                break;
                            }
                        }
                    }

                    diagnostics.push_back(diag);
                }
            }
        }

        // Fallback to v1 ABI if v2 path didn't produce results
        if (diagnostics.empty()) {
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
