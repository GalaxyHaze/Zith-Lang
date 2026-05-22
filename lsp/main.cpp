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

    std::string buffer;

    while (std::getline(std::cin, buffer)) {
        if (buffer.empty()) continue;

        if (buffer.find("Content-Length:") == 0 || buffer.find("content-length:") == 0) {
            size_t contentLength = 0;
            size_t colonPos = buffer.find(':');
            if (colonPos != std::string::npos) {
                contentLength = std::stoul(buffer.substr(colonPos + 1));
            }

            std::string emptyLine;
            std::getline(std::cin, emptyLine);

            std::string body(contentLength, '\0');
            std::cin.read(&body[0], contentLength);

            if (std::cin.gcount() == static_cast<std::streamsize>(contentLength)) {
                try {
                    if (auto response = handler.handleMessage(body);
                        !response.is_null() && !response.empty()) {
                        std::cout << "Content-Length: " << response.dump().size() << "\r\n\r\n" << response.dump() << std::flush;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error handling message: " << e.what() << std::endl;
                    json error;
                    error["jsonrpc"] = "2.0";
                    error["error"] = json::object();
                    error["error"]["code"] = -32603;
                    error["error"]["message"] = e.what();
                    std::cout << "Content-Length: " << error.dump().size() << "\r\n\r\n" << error.dump() << std::flush;
                }
            }
        }
    }

    std::cerr << "Zith LSP shutting down" << std::endl;
    return 0;
}
