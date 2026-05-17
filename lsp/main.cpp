#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <vector>
#include <optional>

#include <nlohmann/json.hpp>

#include "message_handler.h"
#include "document_manager.h"

using json = nlohmann::json;

int main() {
    std::cerr << "Zith LSP starting..." << std::endl;

    DocumentManager docManager;
    MessageHandler handler(docManager);

    std::string content;
    std::string buffer;

    while (std::getline(std::cin, buffer)) {
        content += buffer;
        content += '\n';

        if (buffer == "" || buffer == "Content-Length: 0") {
            continue;
        }

        if (buffer.find("Content-Length:") == 0) {
            size_t contentLength = 0;
            if (sscanf(buffer.c_str(), "Content-Length: %zu", &contentLength) == 1) {
                std::string body;
                while (body.size() < contentLength && std::getline(std::cin, buffer)) {
                    body += buffer;
                    body += '\n';
                }
                if (body.size() == contentLength) {
                    try {
                        auto response = handler.handleMessage(body);
                        if (!response.is_null()) {
                            std::cout << "Content-Length: " << response.dump().size() << "\r\n\r\n" << response.dump() << std::flush;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error handling message: " << e.what() << std::endl;
                        json error = json::object();
                        error["jsonrpc"] = "2.0";
                        error["error"] = json::object();
                        error["error"]["code"] = -32603;
                        error["error"]["message"] = e.what();
                        std::cout << "Content-Length: " << error.dump().size() << "\r\n\r\n" << error.dump() << std::flush;
                    }
                }
            }
            content.clear();
        }
    }

    std::cerr << "Zith LSP shutting down" << std::endl;
    return 0;
}