#include "document_manager.h"
#include <iostream>
#include <algorithm>

void DocumentManager::openDocument(const std::string& uri, const std::string& content) {
    auto doc = std::make_shared<Document>(content);
    doc->uri_ = uri;
    doc->path_ = getPathFromUri(uri);
    documents_[uri] = doc;
    std::cerr << "Opened document: " << doc->path_ << std::endl;
}

void DocumentManager::updateDocument(const std::string& uri, const std::string& content) {
    if (documents_.contains(uri)) {
        documents_[uri]->setContent(content);
        std::cerr << "Updated document: " << documents_[uri]->path_ << std::endl;
    }
}

void DocumentManager::closeDocument(const std::string& uri) {
    documents_.erase(uri);
    std::cerr << "Closed document: " << uri << std::endl;
}

std::shared_ptr<Document> DocumentManager::getDocument(const std::string& uri) const {
    auto it = documents_.find(uri);
    if (it != documents_.end()) {
        return it->second;
    }
    return nullptr;
}

std::string DocumentManager::getPathFromUri(const std::string& uri) const {
    if (uri.find("file://") == 0) {
        std::string path = uri.substr(7);
        if (path.find("%20") != std::string::npos) {
            size_t pos = 0;
            while ((pos = path.find("%20", pos)) != std::string::npos) {
                path.replace(pos, 3, " ");
            }
        }
        return path;
    }
    return uri;
}