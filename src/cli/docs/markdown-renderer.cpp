#include "cli/docs/markdown-renderer.hpp"

#include <algorithm>
#include <string_view>
#include <tuple>
#include <vector>

namespace zith::cli::docs {

namespace {

std::string codeSpan(const std::string_view text) {
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

    const std::string delimiter(longestRun + 1U, '`');
    const bool needsPadding = !text.empty() && (text.front() == '`' || text.back() == '`' ||
                                                (text.front() == ' ' && text.back() == ' '));
    return delimiter + (needsPadding ? " " : "") + std::string(text) + (needsPadding ? " " : "") +
           delimiter;
}

std::string_view visibilityName(const frontend::Visibility visibility) {
    switch (visibility) {
    case frontend::Visibility::Private:
        return "private";
    case frontend::Visibility::Public:
        return "public";
    case frontend::Visibility::Module:
        return "module";
    }
    return "unknown";
}

size_t sectionOrder(const frontend::DeclKind kind) {
    switch (kind) {
    case frontend::DeclKind::Function:
        return 0;
    case frontend::DeclKind::TypeAlias:
        return 1;
    case frontend::DeclKind::Struct:
        return 2;
    case frontend::DeclKind::Enum:
        return 3;
    case frontend::DeclKind::Union:
        return 4;
    case frontend::DeclKind::Trait:
        return 5;
    case frontend::DeclKind::Interface:
        return 6;
    case frontend::DeclKind::Variable:
        return 7;
    case frontend::DeclKind::Context:
        return 8;
    case frontend::DeclKind::Word:
        return 9;
    case frontend::DeclKind::Import:
        return 10;
    case frontend::DeclKind::Macro:
        return 11;
    case frontend::DeclKind::Error:
        return 12;
    }
    return 12;
}

std::string_view sectionName(const frontend::DeclKind kind) {
    switch (kind) {
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

void appendBlock(std::string &output, const std::string &text) {
    if (text.empty())
        return;
    output += text;
    if (!output.ends_with('\n'))
        output += '\n';
    if (!output.ends_with("\n\n"))
        output += '\n';
}

void renderMember(std::string &output, const DocMember &member) {
    output += "###### ";
    if (!member.signature.empty()) {
        output += codeSpan(member.signature);
    } else {
        output += member.kind;
        output += ' ';
        output += codeSpan(member.name);
        if (!member.type.empty()) {
            output += ": ";
            output += codeSpan(member.type);
        }
    }
    if (!member.defaultValue.empty()) {
        output += " = ";
        output += codeSpan(member.defaultValue);
    }
    output += "\n\n- Visibility: ";
    output += visibilityName(member.visibility);
    output += '\n';
    if (member.visibility == frontend::Visibility::Module) {
        output += "- Module depth: ";
        output += std::to_string(member.modDepth);
        output += '\n';
    }
    if (member.isConst)
        output += "- Constant: yes\n";
    output += '\n';
    appendBlock(output, member.documentation);
}

void renderSymbol(std::string &output, const DocSymbol &symbol,
                  const std::string_view containingModule) {
    const auto title = symbol.signature.empty() ? std::string_view(symbol.name)
                                                : std::string_view(symbol.signature);
    output += "##### ";
    output += codeSpan(title);
    output += "\n\n- Visibility: ";
    output += visibilityName(symbol.visibility);
    output += '\n';

    if (symbol.isReexport) {
        output += "- Origin: re-exported from ";
        output += codeSpan(symbol.originModule);
        output += '\n';
    } else {
        output += "- Origin: local to ";
        output += codeSpan(containingModule);
        output += '\n';
    }
    if (!symbol.ownerName.empty()) {
        output += "- Owner: ";
        output += codeSpan(symbol.ownerName);
        output += '\n';
    }
    if (symbol.isExtern)
        output += "- External: yes\n";
    if (!symbol.type.empty()) {
        output += "- Type: ";
        output += codeSpan(symbol.type);
        output += '\n';
    }
    output += '\n';
    appendBlock(output, symbol.documentation);
    for (const auto &member : symbol.members)
        renderMember(output, member);
}

void renderModule(std::string &output, const DocModule &module, const DocModel &model) {
    const bool isEntry = module.key == model.entryModule;
    output += "### ";
    output += codeSpan(module.key);
    if (isEntry)
        output += " (entry)";
    else if (module.isDependency)
        output += " (dependency)";
    output += "\n\n";

    std::vector<const DocSymbol *> symbols;
    symbols.reserve(module.symbols.size());
    for (const auto &symbol : module.symbols)
        symbols.push_back(&symbol);
    std::stable_sort(
        symbols.begin(), symbols.end(), [](const DocSymbol *left, const DocSymbol *right) {
            return std::tuple(sectionOrder(left->kind), left->name, left->ownerName,
                              left->signature, left->originModule, left->isReexport) <
                   std::tuple(sectionOrder(right->kind), right->name, right->ownerName,
                              right->signature, right->originModule, right->isReexport);
        });

    size_t previousSection = static_cast<size_t>(-1);
    for (const auto *symbol : symbols) {
        const auto currentSection = sectionOrder(symbol->kind);
        if (currentSection != previousSection) {
            output += "#### ";
            output += sectionName(symbol->kind);
            output += "\n\n";
            previousSection = currentSection;
        }
        renderSymbol(output, *symbol, module.key);
    }
}

} // namespace

std::string renderMarkdown(const DocModel &model) {
    std::string output = "# API Reference\n\nEntry module: ";
    output += codeSpan(model.entryModule);
    output += "\n\n## Reachable Modules\n\n";

    std::vector<const DocModule *> modules;
    modules.reserve(model.modules.size());
    for (const auto &module : model.modules)
        modules.push_back(&module);
    std::stable_sort(modules.begin(), modules.end(),
                     [&](const DocModule *left, const DocModule *right) {
                         const bool leftIsEntry  = left->key == model.entryModule;
                         const bool rightIsEntry = right->key == model.entryModule;
                         if (leftIsEntry != rightIsEntry)
                             return leftIsEntry;
                         return std::tie(left->key, left->isDependency) <
                                std::tie(right->key, right->isDependency);
                     });

    for (const auto *module : modules) {
        output += "- ";
        output += codeSpan(module->key);
        if (module->key == model.entryModule)
            output += " (entry)";
        else if (module->isDependency)
            output += " (dependency)";
        else
            output += " (project)";
        output += '\n';
    }

    output += "\n## Modules\n\n";
    for (const auto *module : modules)
        renderModule(output, *module, model);
    if (output.ends_with("\n\n"))
        output.pop_back();
    return output;
}

} // namespace zith::cli::docs
