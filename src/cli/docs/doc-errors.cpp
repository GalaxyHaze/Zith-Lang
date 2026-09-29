#include "cli/docs/doc-errors.hpp"

#include <algorithm>
#include <tuple>

namespace zith::cli::docs {

namespace {

const GraphModule *findGraphModule(const ProjectGraph &graph, const std::string_view key) {
    for (const auto &module : graph.modules)
        if (module.artifact && module.artifact->key == key)
            return &module;
    return nullptr;
}

std::string moduleFile(const GraphModule *module) {
    if (module == nullptr || !module->artifact || !module->artifact->source)
        return {};
    return module->artifact->source->canonicalPath;
}

std::string sectionForDeclaration(const frontend::Declaration &declaration) {
    switch (declaration.kind) {
    case frontend::DeclKind::Function:
        return "Functions";
    case frontend::DeclKind::TypeAlias:
        return "Types";
    case frontend::DeclKind::Struct:
        return "Structs";
    case frontend::DeclKind::Enum:
        return "Enums";
    case frontend::DeclKind::Union:
        return "Unions";
    case frontend::DeclKind::Trait:
        return "Traits";
    case frontend::DeclKind::Interface:
        return "Interfaces";
    case frontend::DeclKind::Variable:
        return "Variables";
    case frontend::DeclKind::Context:
        return "Contexts";
    case frontend::DeclKind::Word:
        return "Words";
    case frontend::DeclKind::Import:
        return "Imports";
    case frontend::DeclKind::Macro:
        return "Macros";
    case frontend::DeclKind::Error:
        return "Declarations";
    }
    return "Declarations";
}

std::string sectionForOffset(const GraphModule *module, const uint32_t offset,
                             const std::string_view fallback) {
    if (module == nullptr || !module->artifact || !module->artifact->frontend)
        return std::string(fallback);

    for (const auto &declaration : module->artifact->frontend->declarations())
        if (declaration.span.start <= offset && offset <= declaration.span.end)
            return sectionForDeclaration(declaration);
    return std::string(fallback);
}

const GraphModule *findModuleForFile(const ProjectGraph &graph, const memory::Span span,
                                     const std::string_view file) {
    for (const auto &module : graph.modules) {
        if (!module.artifact)
            continue;
        if (module.artifact->fileId == span.file || module.artifact->key == file ||
            (module.artifact->source && module.artifact->source->canonicalPath == file))
            return &module;
    }
    return nullptr;
}

std::string fileForSpan(session::CompilationSession &session, const memory::Span span) {
    if (!session.sourceMap().isValid(span.file))
        return {};
    auto source = session.sourceMap().get(span.file);
    return source ? source->get().path : std::string{};
}

std::string markdownCodeSpan(const std::string_view text) {
    size_t longestRun = 0;
    size_t currentRun = 0;
    for (const char character : text) {
        if (character == '`') {
            ++currentRun;
            longestRun = std::max(longestRun, currentRun);
        } else {
            currentRun = 0;
        }
    }
    const std::string delimiter(longestRun + 1, '`');
    return delimiter + std::string(text) + delimiter;
}

} // namespace

std::vector<DocError> collectDocErrors(session::CompilationSession &session,
                                       const ProjectGraph &graph, const DocModel &model) {
    std::vector<DocError> result;
    for (const auto &diagnostic : session.diags().all()) {
        if (!diagnostic.isError())
            continue;

        const auto file    = fileForSpan(session, diagnostic.primary);
        const auto *module = findModuleForFile(graph, diagnostic.primary, file);
        const auto key     = module && module->artifact ? module->artifact->key : std::string{};
        result.push_back({file, key, sectionForOffset(module, diagnostic.primary.start, "Source"),
                          diagnostic.message, diagnostic.primary.start, diagnostic.primary.end});
    }

    for (const auto &error : model.errors) {
        const auto *module = findGraphModule(graph, error.module);
        result.push_back({moduleFile(module), error.module,
                          sectionForOffset(module, error.span.start, "Documentation"),
                          error.message, error.span.start, error.span.end});
    }

    if (!graph.ok()) {
        const auto *entry = findGraphModule(graph, graph.entryModule);
        result.push_back({moduleFile(entry), graph.entryModule, "Module graph", graph.error, 0, 0});
    }

    std::sort(result.begin(), result.end(), [](const DocError &left, const DocError &right) {
        return std::tie(left.file, left.module, left.section, left.start, left.end, left.message) <
               std::tie(right.file, right.module, right.section, right.start, right.end,
                        right.message);
    });
    result.erase(std::unique(result.begin(), result.end(),
                             [](const DocError &left, const DocError &right) {
                                 return left.file == right.file && left.module == right.module &&
                                        left.section == right.section &&
                                        left.message == right.message &&
                                        left.start == right.start && left.end == right.end;
                             }),
                 result.end());
    return result;
}

std::string renderDocErrors(const std::vector<DocError> &errors) {
    if (errors.empty())
        return {};

    std::string result = "## Errors\n\n";
    std::string previousFile;
    std::string previousModule;
    std::string previousSection;
    for (const auto &error : errors) {
        const auto file    = error.file.empty() ? std::string("(unknown)") : error.file;
        const auto module  = error.module.empty() ? std::string("(unknown)") : error.module;
        const auto section = error.section.empty() ? std::string("Source") : error.section;
        if (file != previousFile) {
            result += "### File: " + markdownCodeSpan(file) + "\n\n";
            previousFile    = file;
            previousModule  = {};
            previousSection = {};
        }
        if (module != previousModule) {
            result += "#### Module: " + markdownCodeSpan(module) + "\n\n";
            previousModule  = module;
            previousSection = {};
        }
        if (section != previousSection) {
            result += "##### " + section + "\n";
            previousSection = section;
        }
        result += "- [" + std::to_string(error.start) + ".." + std::to_string(error.end) + "] ";
        result += markdownCodeSpan(error.message) + "\n";
    }
    return result + "\n";
}

} // namespace zith::cli::docs
