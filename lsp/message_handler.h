#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "document_manager.h"
#include "handlers/handler_interface.h"

using json = nlohmann::json;

class MessageHandler {
public:
    explicit MessageHandler(DocumentManager& docManager);

    json handleMessage(const std::string& message);

private:
    DocumentManager& docManager_;
    std::unordered_map<std::string, std::function<json(const json&)>> handlers_;
    int id_ = 0;

    void initHandlers();
    json handleInitialize(const json& params);
    json handleTextDocumentDidOpen(const json& params);
    json handleTextDocumentDidChange(const json& params);
    json handleTextDocumentDidClose(const json& params);
    json handleTextDocumentDefinition(const json& params);
    json handleTextDocumentHover(const json& params);
    json handleSemanticTokensFull(const json& params);
    json handleExecuteCommand(const json& params);
    json handleShutdown(const json& params);

    void publishDiagnostics(const std::string& uri);
};

json handleNotification(const json& message, DocumentManager& docManager);