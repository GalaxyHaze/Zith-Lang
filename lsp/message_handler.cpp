#include "message_handler.h"
#include <iostream>
#include <optional>

MessageHandler::MessageHandler(DocumentManager& docManager)
    : docManager_(docManager) {
    initHandlers();
}

void MessageHandler::initHandlers() {
    handlers_["initialize"] = [this](const json& params) { return handleInitialize(params); };
    handlers_["shutdown"] = [this](const json& params) { return handleShutdown(params); };
    handlers_["textDocument/didOpen"] = [this](const json& params) { return handleTextDocumentDidOpen(params); };
    handlers_["textDocument/didChange"] = [this](const json& params) { return handleTextDocumentDidChange(params); };
    handlers_["textDocument/definition"] = [this](const json& params) { return handleTextDocumentDefinition(params); };
    handlers_["textDocument/hover"] = [this](const json& params) { return handleTextDocumentHover(params); };
}

json MessageHandler::handleMessage(const std::string& message) {
    try {
        json msg = json::parse(message);

        std::string method;
        json id;
        json params;

        if (msg.contains("method")) {
            method = msg["method"];

            if (msg.contains("id")) {
                id = msg["id"];
            }

            if (msg.contains("params")) {
                params = msg["params"];
            }

            if (method == "initialize") {
                json result = handleInitialize(params);
                json response;
                response["jsonrpc"] = "2.0";
                response["id"] = id;
                response["result"] = result;
                return response;
            }

            if (method == "initialized") {
                return json::object();
            }

            if (method == "shutdown") {
                json result = handleShutdown(params);
                json response;
                response["jsonrpc"] = "2.0";
                response["id"] = id;
                response["result"] = result;
                return response;
            }

            if (method == "exit") {
                return json::object();
            }

            if (handlers_.contains(method)) {
                json result = handlers_[method](params);

                if (id.is_number()) {
                    json response;
                    response["jsonrpc"] = "2.0";
                    response["id"] = id;
                    response["result"] = result;
                    return response;
                }
            } else {
                std::cerr << "Unknown method: " << method << std::endl;
            }
        }
    } catch (const json::parse_error& e) {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
    }

    return json::object();
}

json MessageHandler::handleInitialize(const json& params) {
    json capabilities;
    capabilities["textDocumentSync"] = 1;  // Full document sync
    capabilities["definitionProvider"] = true;
    capabilities["hoverProvider"] = true;

    json serverInfo;
    serverInfo["name"] = "Zith LSP";
    serverInfo["version"] = "0.0.1";

    json result;
    result["capabilities"] = capabilities;
    result["serverInfo"] = serverInfo;

    return result;
}

json MessageHandler::handleShutdown(const json& params) {
    return json::object();
}

json MessageHandler::handleTextDocumentDidOpen(const json& params) {
    if (!params.contains("textDocument")) return json::object();

    auto textDoc = params["textDocument"];
    std::string uri = textDoc["uri"];
    std::string content = textDoc["text"];

    docManager_.openDocument(uri, content);

    auto diagnostics = runDiagnostics(docManager_, uri);
    if (!diagnostics.empty()) {
        json notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = "textDocument/publishDiagnostics";
        notification["params"] = json::object();
        notification["params"]["uri"] = uri;
        notification["params"]["diagnostics"] = diagnostics;
        std::cout << "Content-Length: " << notification.dump().size() << "\r\n\r\n" << notification.dump() << std::flush;
    }

    return json::object();
}

json MessageHandler::handleTextDocumentDidChange(const json& params) {
    if (!params.contains("textDocument") || !params.contains("contentChanges")) {
        return json::object();
    }

    auto textDoc = params["textDocument"];
    std::string uri = textDoc["uri"];
    auto changes = params["contentChanges"];

    if (!changes.empty() && changes[0].contains("text")) {
        docManager_.updateDocument(uri, changes[0]["text"]);
    }

    auto diagnostics = runDiagnostics(docManager_, uri);
    if (!diagnostics.empty()) {
        json notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = "textDocument/publishDiagnostics";
        notification["params"] = json::object();
        notification["params"]["uri"] = uri;
        notification["params"]["diagnostics"] = diagnostics;
        std::cout << "Content-Length: " << notification.dump().size() << "\r\n\r\n" << notification.dump() << std::flush;
    }

    return json::object();
}

json MessageHandler::handleTextDocumentDefinition(const json& params) {
    if (!params.contains("textDocument") || !params.contains("position")) {
        return json::null();
    }

    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int character = params["position"]["character"];

    return findDefinition(docManager_, uri, line, character);
}

json MessageHandler::handleTextDocumentHover(const json& params) {
    if (!params.contains("textDocument") || !params.contains("position")) {
        return json::null();
    }

    std::string uri = params["textDocument"]["uri"];
    int line = params["position"]["line"];
    int character = params["position"]["character"];

    return findHover(docManager_, uri, line, character);
}