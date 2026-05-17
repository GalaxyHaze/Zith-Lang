#pragma once

#include <string>
#include <unordered_map>
#include <memory>

class Document {
public:
    Document(std::string content)
        : content_(std::move(content)), version_(1) {}

    const std::string& getContent() const { return content_; }
    int getVersion() const { return version_; }

    void setContent(std::string content) {
        content_ = std::move(content);
        version_++;
    }

    std::string uri_;
    std::string path_;

private:
    std::string content_;
    int version_;
};

class DocumentManager {
public:
    void openDocument(const std::string& uri, const std::string& content);
    void updateDocument(const std::string& uri, const std::string& content);
    void closeDocument(const std::string& uri);

    std::shared_ptr<Document> getDocument(const std::string& uri) const;
    std::string getPathFromUri(const std::string& uri) const;

private:
    std::unordered_map<std::string, std::shared_ptr<Document>> documents_;
};