#pragma once

#include "cli/docs/project-graph.hpp"
#include "frontend/frontend.hpp"
#include "session/frontend-context.hpp"

#include <string>
#include <vector>

namespace zith::cli::docs {

enum class DocsMode { Interface, Spec };

struct DocMember {
    std::string kind;
    std::string name;
    std::string type;
    std::string defaultValue;
    std::string documentation;
    frontend::Visibility visibility = frontend::Visibility::Private;
    frontend::TextSpan span{};
    int32_t modDepth = 0;
    bool isConst     = false;
    std::string signature;
};

struct DocSymbol {
    frontend::DeclKind kind         = frontend::DeclKind::Error;
    frontend::Visibility visibility = frontend::Visibility::Private;
    frontend::TextSpan span{};
    std::string name;
    std::string ownerName;
    std::string signature;
    std::string type;
    std::string documentation;
    std::string originModule;
    bool isExtern   = false;
    bool isReexport = false;
    std::vector<DocMember> members;
};

struct DocModule {
    std::string key;
    bool isDependency = false;
    std::vector<DocSymbol> symbols;
};

struct DocModelError {
    std::string module;
    frontend::TextSpan span{};
    std::string message;
};

struct DocModel {
    std::string entryModule;
    std::vector<DocModule> modules;
    std::vector<DocModelError> errors;
};

/// Extracts a stable documentation model from the reachable project graph.
[[nodiscard]] DocModel buildDocModel(const session::CompilationSnapshot &snapshot,
                                     const ProjectGraph &graph, DocsMode mode);

} // namespace zith::cli::docs
