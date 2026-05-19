#include "handler_interface.h"
#include "../document_manager.h"
#include <cstdio>
#include <memory>
#include <array>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

static std::string execAndCapture(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;

#if defined(_WIN32)
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), &_pclose);
#else
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), &pclose);
#endif
    if (!pipe) {
        return "[error] Failed to run command";
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

static std::string findZithBinary() {
#if defined(_WIN32)
    const char* searchPaths[] = {
        ".\\zith.exe",
        "..\\zith.exe",
        "..\\..\\zith.exe",
        ".\\build\\zith.exe",
        "..\\build\\zith.exe",
    };
#else
    const char* searchPaths[] = {
        "./zith",
        "../zith",
        "../../zith",
        "./build/zith",
        "../build/zith",
    };
#endif
    for (const auto* path : searchPaths) {
        FILE* f = nullptr;
#if defined(_WIN32)
        fopen_s(&f, path, "rb");
#else
        f = fopen(path, "rb");
#endif
        if (f) {
            fclose(f);
            return path;
        }
    }
    return "zith";
}

json executeCompilerCommand(DocumentManager& docManager, const std::string& command, const json& params) {
    auto doc = docManager.getDocument(params.value("uri", ""));
    std::string filepath = doc ? doc->path_ : "";

    std::string zithBin = findZithBinary();

    std::string cmd;
    std::string actionName;

    if (command == "zith.check") {
        cmd = zithBin + " check";
        actionName = "Check";
    } else if (command == "zith.compile") {
        cmd = zithBin + " compile";
        actionName = "Compile";
    } else if (command == "zith.build") {
        cmd = zithBin + " build";
        actionName = "Build";
    } else {
        return json::object();
    }

    if (!filepath.empty()) {
        cmd += " \"" + filepath + "\"";
    }

    std::string output = execAndCapture(cmd);

    json showMessage;
    showMessage["jsonrpc"] = "2.0";
    showMessage["method"] = "window/showMessage";

    json msgParams;
    if (output.empty()) {
        msgParams["type"] = 3;
        msgParams["message"] = actionName + " completed successfully.";
    } else {
        bool hasError = output.find("error") != std::string::npos ||
                        output.find("Error") != std::string::npos;
        msgParams["type"] = hasError ? 1 : 3;
        msgParams["message"] = actionName + ":\n" + output;
    }
    showMessage["params"] = msgParams;

    std::cout << "Content-Length: " << showMessage.dump().size() << "\r\n\r\n" << showMessage.dump() << std::flush;

    return json::object();
}
