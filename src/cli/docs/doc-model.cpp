#include "cli/docs/doc-model.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace zith::cli::docs {

namespace {

using UsedSymbol = std::pair<std::string, uint32_t>;

std::string_view trimRight(std::string_view text) {
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' ||
                             text.back() == '\n'))
        text.remove_suffix(1);
    return text;
}

std::string_view sourceText(const frontend::FrontendSnapshot &frontend,
                            const frontend::TextSpan span) {
    const auto &source = frontend.source();
    if (span.start > span.end || span.end > source.size())
        return {};
    return std::string_view(source).substr(span.start, span.size());
}

std::string normalizeDocTrivia(const frontend::FrontendSnapshot &frontend,
                               const frontend::Trivia &trivia) {
    auto raw = sourceText(frontend, trivia.span);
    if (trivia.kind == frontend::TriviaKind::DocLine) {
        if (raw.starts_with("///"))
            raw.remove_prefix(3);
        if (!raw.empty() && (raw.front() == ' ' || raw.front() == '\t'))
            raw.remove_prefix(1);
        return std::string(trimRight(raw));
    }

    if (raw.starts_with("/**"))
        raw.remove_prefix(3);
    if (raw.ends_with("*/"))
        raw.remove_suffix(2);

    std::vector<std::string> lines;
    while (true) {
        const auto newline = raw.find('\n');
        auto line          = newline == std::string_view::npos ? raw : raw.substr(0, newline);
        size_t marker      = 0;
        while (marker < line.size() && (line[marker] == ' ' || line[marker] == '\t'))
            ++marker;
        if (marker < line.size() && line[marker] == '*') {
            line.remove_prefix(marker + 1);
            if (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                line.remove_prefix(1);
        }
        lines.emplace_back(trimRight(line));
        if (newline == std::string_view::npos)
            break;
        raw.remove_prefix(newline + 1);
    }

    while (!lines.empty() && lines.front().empty())
        lines.erase(lines.begin());
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();

    size_t commonIndent = std::string::npos;
    for (const auto &line : lines) {
        size_t indent = 0;
        while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t'))
            ++indent;
        if (indent < line.size())
            commonIndent = std::min(commonIndent, indent);
    }
    if (commonIndent != std::string::npos && commonIndent != 0)
        for (auto &line : lines)
            line.erase(0, std::min(commonIndent, line.size()));

    std::string result;
    for (const auto &line : lines) {
        if (!result.empty())
            result += '\n';
        result += line;
    }
    return result;
}

std::string_view tokenText(const frontend::FrontendSnapshot &frontend,
                           const frontend::Token &token) {
    return sourceText(frontend, token.span);
}

bool isDeclarationPrefix(const std::string_view text) {
    return text == "pub" || text == "mod" || text == "raw" || text == "extern" || text == "const" ||
           text == "state" || text == "tag" || text == "unsafe" || text == "async";
}

size_t declarationStartToken(const frontend::FrontendSnapshot &frontend,
                             const frontend::TextSpan span) {
    const auto &tokens = frontend.tokens();
    size_t startIndex  = tokens.size();
    for (size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].kind == frontend::TokenKind::End)
            break;
        if (tokens[index].span.start >= span.start && tokens[index].span.start < span.end) {
            startIndex = index;
            break;
        }
    }
    if (startIndex == tokens.size())
        return startIndex;

    bool moved = true;
    while (moved && startIndex > 0) {
        moved = false;
        if (isDeclarationPrefix(tokenText(frontend, tokens[startIndex - 1]))) {
            --startIndex;
            moved = true;
        }
        if (startIndex > 0 && tokenText(frontend, tokens[startIndex - 1]) == "]") {
            size_t attributeStart = startIndex - 1;
            size_t depth          = 1;
            while (attributeStart > 0 && depth != 0) {
                --attributeStart;
                const auto text = tokenText(frontend, tokens[attributeStart]);
                if (text == "]")
                    ++depth;
                else if (text == "[")
                    --depth;
            }
            if (depth == 0 && attributeStart > 0 &&
                tokenText(frontend, tokens[attributeStart - 1]) == "#") {
                startIndex = attributeStart - 1;
                moved      = true;
            }
        }
    }
    return startIndex;
}

std::string documentationAt(const frontend::FrontendSnapshot &frontend,
                            const frontend::TextSpan span) {
    const size_t startIndex = declarationStartToken(frontend, span);
    if (startIndex >= frontend.tokens().size())
        return {};
    const auto &firstToken = frontend.tokens()[startIndex];

    std::string result;
    const auto triviaEnd =
        static_cast<size_t>(firstToken.leadingTriviaStart) + firstToken.leadingTriviaCount;
    if (triviaEnd > frontend.trivia().size())
        return {};
    for (size_t index = firstToken.leadingTriviaStart; index < triviaEnd; ++index) {
        const auto &trivia = frontend.trivia()[index];
        if (trivia.kind != frontend::TriviaKind::DocLine &&
            trivia.kind != frontend::TriviaKind::DocBlock)
            continue;
        const auto text = normalizeDocTrivia(frontend, trivia);
        if (text.empty())
            continue;
        if (!result.empty())
            result += '\n';
        result += text;
    }
    return result;
}

std::string typeText(const frontend::FrontendSnapshot &frontend, const frontend::TypeExprId type) {
    if (!type || type.value > frontend.typeExpressions().size())
        return {};
    const auto &expression = frontend.typeExpressions()[type.value - 1U];
    return std::string(sourceText(frontend, expression.span));
}

std::string genericParameters(const frontend::FrontendSnapshot &frontend,
                              const std::vector<frontend::GenericParam> &parameters) {
    if (parameters.empty())
        return {};
    std::string result = "<";
    for (size_t index = 0; index < parameters.size(); ++index) {
        if (index != 0)
            result += ", ";
        const auto &parameter = parameters[index];
        result += parameter.name;
        if (!parameter.constraints.empty()) {
            result += ": ";
            for (size_t constraint = 0; constraint < parameter.constraints.size(); ++constraint) {
                if (constraint != 0)
                    result += " + ";
                result += typeText(frontend, parameter.constraints[constraint]);
            }
        }
    }
    result += '>';
    return result;
}

std::string formatFunctionSignature(const frontend::FrontendSnapshot &frontend,
                                    const frontend::Declaration &declaration) {
    std::string result;
    switch (declaration.functionKind) {
    case frontend::FunctionKind::Standard:
        result = "fn ";
        break;
    case frontend::FunctionKind::Const:
        result = "const fn ";
        break;
    case frontend::FunctionKind::Raw:
        result = "raw fn ";
        break;
    case frontend::FunctionKind::Extern:
        result = "extern fn ";
        break;
    case frontend::FunctionKind::State:
        result = "state ";
        break;
    }
    result += declaration.name;
    result += genericParameters(frontend, declaration.genericParams);
    result += '(';
    for (size_t index = 0; index < declaration.parameters.size(); ++index) {
        if (index != 0)
            result += ", ";
        const auto &parameter = declaration.parameters[index];
        if (parameter.bindingKind == frontend::BindingKind::Var)
            result += "var ";
        else if (parameter.bindingKind == frontend::BindingKind::Let)
            result += "let ";
        result += parameter.name;
        const auto type = typeText(frontend, parameter.type);
        if (!type.empty()) {
            result += ": ";
            result += type;
        }
        if (parameter.defaultValue &&
            parameter.defaultValue.value <= frontend.expressions().size()) {
            result += " = ";
            result += sourceText(frontend,
                                 frontend.expressions()[parameter.defaultValue.value - 1U].span);
        }
    }
    if (declaration.isVariadic) {
        if (!declaration.parameters.empty())
            result += ", ";
        result += "...";
    }
    result += ')';
    const auto returnType = typeText(frontend, declaration.declaredType);
    if (!returnType.empty()) {
        result += ": ";
        result += returnType;
    }
    return result;
}

std::string declarationType(const frontend::FrontendSnapshot &frontend,
                            const frontend::Declaration &declaration) {
    if (declaration.kind == frontend::DeclKind::Function)
        return typeText(frontend, declaration.declaredType);
    return typeText(frontend, declaration.declaredType);
}

DocMember makeMember(const frontend::FrontendSnapshot &frontend,
                     const frontend::Parameter &parameter, const frontend::DeclKind kind) {
    DocMember member;
    member.kind          = kind == frontend::DeclKind::Enum ? "variant" : "field";
    member.name          = parameter.name;
    member.type          = typeText(frontend, parameter.type);
    member.modDepth      = parameter.modDepth;
    member.isConst       = parameter.isConstField;
    member.visibility    = parameter.visibility;
    member.span          = parameter.span;
    member.documentation = documentationAt(frontend, parameter.span);
    if (kind == frontend::DeclKind::Interface)
        member.visibility = frontend::Visibility::Public;
    if (kind == frontend::DeclKind::Union) {
        member.kind = "alternative";
        member.name = member.type;
        member.type.clear();
    }
    if (parameter.defaultValue && parameter.defaultValue.value <= frontend.expressions().size())
        member.defaultValue = std::string(
            sourceText(frontend, frontend.expressions()[parameter.defaultValue.value - 1U].span));
    if (parameter.isConstField)
        member.kind = "const field";
    return member;
}

DocMember makeMethodMember(const frontend::FrontendSnapshot &frontend,
                           const frontend::Declaration &declaration) {
    DocMember member;
    member.kind          = "method";
    member.name          = declaration.name;
    member.signature     = formatFunctionSignature(frontend, declaration);
    member.documentation = documentationAt(frontend, declaration.span);
    member.visibility    = frontend::Visibility::Public;
    member.span          = declaration.span;
    return member;
}

DocSymbol makeSymbol(const session::ModuleArtifact &module,
                     const frontend::Declaration &declaration) {
    const auto &frontend = *module.frontend;
    DocSymbol symbol;
    symbol.kind       = declaration.kind;
    symbol.visibility = declaration.visibility;
    symbol.span       = declaration.span;
    symbol.name       = declaration.name;
    symbol.ownerName =
        declaration.ownerName.empty() ? declaration.parentName : declaration.ownerName;
    symbol.originModule  = module.key;
    symbol.isExtern      = declaration.isExtern;
    symbol.documentation = documentationAt(frontend, declaration.span);

    if (declaration.kind == frontend::DeclKind::Function ||
        declaration.kind == frontend::DeclKind::Macro) {
        if (declaration.kind == frontend::DeclKind::Function) {
            symbol.signature = formatFunctionSignature(frontend, declaration);
        } else {
            if (declaration.isRawMacro)
                symbol.signature = "raw ";
            else if (declaration.isTagMacro)
                symbol.signature = "tag ";
            symbol.signature += "macro ";
            symbol.signature += declaration.name;
            symbol.signature += genericParameters(frontend, declaration.genericParams);
        }
        if (declaration.kind == frontend::DeclKind::Macro) {
            symbol.signature += '(';
            for (size_t index = 0; index < declaration.parameters.size(); ++index) {
                if (index != 0)
                    symbol.signature += ", ";
                symbol.signature += declaration.parameters[index].name;
                const auto type = typeText(frontend, declaration.parameters[index].type);
                if (!type.empty()) {
                    symbol.signature += ": ";
                    symbol.signature += type;
                }
            }
            symbol.signature += ')';
        }
    } else if (declaration.kind == frontend::DeclKind::Import) {
        symbol.signature = declaration.import.isExport ? "export " : "import ";
        symbol.signature += declaration.import.rawPath;
        if (!declaration.import.alias.empty()) {
            symbol.signature += " as ";
            symbol.signature += declaration.import.alias;
        }
        symbol.isReexport = declaration.import.isExport;
    } else if (declaration.kind == frontend::DeclKind::Variable) {
        symbol.signature = "const " + declaration.name;
        symbol.type      = typeText(frontend, declaration.declaredType);
    } else {
        symbol.type = declarationType(frontend, declaration);
        switch (declaration.kind) {
        case frontend::DeclKind::TypeAlias:
            symbol.signature = declaration.isNominalType ? "type " : "alias ";
            break;
        case frontend::DeclKind::Struct:
            symbol.signature = "struct ";
            break;
        case frontend::DeclKind::Enum:
            symbol.signature = "enum ";
            break;
        case frontend::DeclKind::Union:
            symbol.signature = declaration.isRawUnion ? "raw union " : "union ";
            break;
        case frontend::DeclKind::Trait:
            symbol.signature = "trait ";
            break;
        case frontend::DeclKind::Interface:
            symbol.signature = "interface ";
            break;
        case frontend::DeclKind::Context:
            symbol.signature = "context ";
            break;
        case frontend::DeclKind::Word:
            symbol.signature = "word ";
            break;
        default:
            break;
        }
        symbol.signature += declaration.name;
        symbol.signature += genericParameters(frontend, declaration.genericParams);
    }

    if (declaration.kind == frontend::DeclKind::Struct ||
        declaration.kind == frontend::DeclKind::Enum ||
        declaration.kind == frontend::DeclKind::Union ||
        declaration.kind == frontend::DeclKind::Interface) {
        symbol.members.reserve(declaration.parameters.size());
        for (const auto &parameter : declaration.parameters)
            symbol.members.push_back(makeMember(frontend, parameter, declaration.kind));
    }
    if (declaration.kind == frontend::DeclKind::Trait ||
        declaration.kind == frontend::DeclKind::Interface) {
        for (const auto &member : frontend.declarations()) {
            if (member.kind == frontend::DeclKind::Function &&
                member.ownerName == declaration.name && member.traitName == declaration.name)
                symbol.members.push_back(makeMethodMember(frontend, member));
        }
    }
    std::stable_sort(symbol.members.begin(), symbol.members.end(),
                     [](const DocMember &left, const DocMember &right) {
                         return std::tie(left.kind, left.name, left.signature, left.span.start) <
                                std::tie(right.kind, right.name, right.signature, right.span.start);
                     });
    if (!declaration.externalSymbol.empty()) {
        symbol.signature += " = extern ";
        symbol.signature += declaration.externalSymbol;
    }
    return symbol;
}

bool hasKind(const DocModule &module, const DocSymbol &candidate) {
    return std::any_of(module.symbols.begin(), module.symbols.end(), [&](const DocSymbol &symbol) {
        return symbol.kind == candidate.kind && symbol.name == candidate.name &&
               symbol.ownerName == candidate.ownerName && symbol.signature == candidate.signature &&
               symbol.originModule == candidate.originModule &&
               symbol.isReexport == candidate.isReexport && symbol.span == candidate.span;
    });
}

bool isNamedType(const frontend::DeclKind kind) {
    switch (kind) {
    case frontend::DeclKind::TypeAlias:
    case frontend::DeclKind::Struct:
    case frontend::DeclKind::Enum:
    case frontend::DeclKind::Union:
    case frontend::DeclKind::Trait:
    case frontend::DeclKind::Interface:
    case frontend::DeclKind::Context:
    case frontend::DeclKind::Word:
        return true;
    default:
        return false;
    }
}

void markUsedTypeSymbols(const session::CompilationSnapshot &snapshot,
                         const session::ModuleArtifact &sourceModule,
                         const session::ModuleResolution &resolution,
                         const frontend::TypeExprId typeId, std::set<UsedSymbol> &usedSymbols) {
    const auto &sourceFrontend = *sourceModule.frontend;
    if (!typeId || typeId.value > sourceFrontend.typeExpressions().size())
        return;

    const auto &type = sourceFrontend.typeExpressions()[typeId.value - 1U];
    if (type.kind == frontend::TypeExprKind::Name) {
        if (type.segments.empty()) {
            const auto *binding = session::lookupBinding(resolution, type.name, frontend::ScopeId{},
                                                         sourceFrontend.scopes());
            if (binding != nullptr && binding->kind == session::ResolutionKind::Import &&
                !binding->target.module.empty() && binding->target.localSymbol)
                usedSymbols.emplace(binding->target.module, binding->target.localSymbol.value);
        } else {
            const auto *alias = session::lookupModuleAliasForPath(
                resolution, type.segments.front(), frontend::ScopeId{}, sourceFrontend.scopes(),
                type.segments);
            const auto *target = alias != nullptr && !alias->target.module.empty()
                                     ? snapshot.findModule(alias->target.module)
                                     : nullptr;
            if (target != nullptr) {
                const auto &symbolName = type.segments.back();
                for (const auto &symbol : target->publicSymbols)
                    if (symbol.name == symbolName && isNamedType(symbol.kind))
                        usedSymbols.emplace(target->key, symbol.id.value);
            }
        }
    }

    for (const auto argument : type.arguments)
        markUsedTypeSymbols(snapshot, sourceModule, resolution, argument, usedSymbols);
}

void markUsedSymbols(const session::CompilationSnapshot &snapshot, const ProjectGraph &graph,
                     std::set<UsedSymbol> &usedSymbols) {
    for (const auto &module : graph.modules) {
        if (module.isDependency)
            continue;
        const auto *resolution = snapshot.findResolution(module.artifact->key);
        if (resolution == nullptr)
            continue;
        const auto append = [&](const session::ResolvedName &name) {
            if (!name.target.module.empty() && name.target.localSymbol)
                usedSymbols.emplace(name.target.module, name.target.localSymbol.value);
        };
        for (const auto &expression : resolution->expressions)
            append(expression);

        const auto &frontend = *module.artifact->frontend;
        for (const auto &declaration : frontend.declarations()) {
            markUsedTypeSymbols(snapshot, *module.artifact, *resolution, declaration.declaredType,
                                usedSymbols);
            for (const auto &parameter : declaration.parameters)
                markUsedTypeSymbols(snapshot, *module.artifact, *resolution, parameter.type,
                                    usedSymbols);
            for (const auto &parameter : declaration.genericParams) {
                markUsedTypeSymbols(snapshot, *module.artifact, *resolution, parameter.constraint,
                                    usedSymbols);
                for (const auto constraint : parameter.constraints)
                    markUsedTypeSymbols(snapshot, *module.artifact, *resolution, constraint,
                                        usedSymbols);
            }
        }
    }
}

bool declarationIsUsed(const session::ModuleArtifact &module,
                       const frontend::Declaration &declaration,
                       const std::set<UsedSymbol> &usedSymbols) {
    const auto isUsedInfo = [&](const session::LocalSymbolInfo &info) {
        return info.span == declaration.span && usedSymbols.contains({module.key, info.id.value});
    };
    return std::any_of(module.publicSymbols.begin(), module.publicSymbols.end(), isUsedInfo) ||
           std::any_of(module.moduleSymbols.begin(), module.moduleSymbols.end(), isUsedInfo);
}

const session::ImportEdge *edgeForImport(const session::CompilationSnapshot &snapshot,
                                         const std::string_view module,
                                         const frontend::Declaration &declaration) {
    for (const auto &edge : snapshot.importGraph())
        if (edge.importer == module && edge.request.span.start == declaration.span.start)
            return &edge;
    return nullptr;
}

std::string reexportedName(const frontend::ImportDecl &import, const std::string_view name) {
    if (import.selectors.empty())
        return std::string(name);
    for (const auto &selector : import.selectors)
        if (selector.name == name)
            return selector.alias.empty() ? selector.name : selector.alias;
    return {};
}

bool isLocalState(const frontend::Declaration &declaration) {
    return declaration.parentScope && declaration.kind == frontend::DeclKind::Function &&
           declaration.functionKind == frontend::FunctionKind::State;
}

bool isTraitRequirement(const frontend::Declaration &declaration) {
    return declaration.kind == frontend::DeclKind::Function && !declaration.ownerName.empty() &&
           declaration.ownerName == declaration.traitName;
}

bool hasEquivalentSymbol(const std::vector<DocSymbol> &symbols, const DocSymbol &candidate) {
    return std::any_of(symbols.begin(), symbols.end(), [&](const DocSymbol &symbol) {
        return symbol.kind == candidate.kind && symbol.name == candidate.name &&
               symbol.ownerName == candidate.ownerName && symbol.signature == candidate.signature &&
               symbol.originModule == candidate.originModule &&
               symbol.isReexport == candidate.isReexport && symbol.span == candidate.span;
    });
}

} // namespace

DocModel buildDocModel(const session::CompilationSnapshot &snapshot, const ProjectGraph &graph,
                       const DocsMode mode) {
    DocModel result;
    result.entryModule = graph.entryModule;
    if (!graph.ok()) {
        result.errors.push_back({graph.entryModule, {}, graph.error});
        return result;
    }

    std::map<std::string, std::vector<DocSymbol>, std::less<>> allSymbols;
    for (const auto &graphModule : graph.modules) {
        if (!graphModule.artifact || !graphModule.artifact->frontend) {
            result.errors.push_back(
                {graphModule.artifact ? graphModule.artifact->key : std::string{},
                 {},
                 "module has no frontend snapshot"});
            continue;
        }
        auto &symbols = allSymbols[graphModule.artifact->key];
        for (const auto &declaration : graphModule.artifact->frontend->declarations()) {
            if (declaration.parentScope && !isLocalState(declaration))
                continue;
            if (declaration.kind == frontend::DeclKind::Error) {
                result.errors.push_back({graphModule.artifact->key, declaration.span,
                                         "cannot represent malformed declaration"});
                continue;
            }
            symbols.push_back(makeSymbol(*graphModule.artifact, declaration));
        }
    }

    for (size_t pass = 0; pass < graph.modules.size(); ++pass) {
        bool changed = false;
        for (const auto &graphModule : graph.modules) {
            if (!graphModule.artifact || !graphModule.artifact->frontend)
                continue;
            for (const auto &declaration : graphModule.artifact->frontend->declarations()) {
                if (declaration.kind != frontend::DeclKind::Import || !declaration.import.isExport)
                    continue;
                const auto *edge = edgeForImport(snapshot, graphModule.artifact->key, declaration);
                if (edge == nullptr || edge->targetKind != session::ImportTargetKind::Zith)
                    continue;
                auto &exports = allSymbols[graphModule.artifact->key];
                for (const auto &target : edge->targets) {
                    const auto targetSymbols = allSymbols.find(target);
                    if (targetSymbols == allSymbols.end())
                        continue;
                    for (auto reexport : targetSymbols->second) {
                        if (reexport.kind == frontend::DeclKind::Import ||
                            reexport.visibility != frontend::Visibility::Public)
                            continue;
                        const auto exportedName = reexportedName(declaration.import, reexport.name);
                        if (exportedName.empty())
                            continue;
                        reexport.name       = exportedName;
                        reexport.visibility = frontend::Visibility::Public;
                        reexport.isReexport = true;
                        if (!hasEquivalentSymbol(exports, reexport)) {
                            exports.push_back(std::move(reexport));
                            changed = true;
                        }
                    }
                }
            }
        }
        if (!changed)
            break;
    }

    std::set<UsedSymbol> usedSymbols;
    markUsedSymbols(snapshot, graph, usedSymbols);

    result.modules.reserve(graph.modules.size());
    for (const auto &graphModule : graph.modules) {
        if (!graphModule.artifact || !graphModule.artifact->frontend)
            continue;
        DocModule module;
        module.key          = graphModule.artifact->key;
        module.isDependency = graphModule.isDependency;

        for (const auto &declaration : graphModule.artifact->frontend->declarations()) {
            if ((declaration.parentScope && !isLocalState(declaration)) ||
                declaration.kind == frontend::DeclKind::Error || isTraitRequirement(declaration))
                continue;
            auto symbol = makeSymbol(*graphModule.artifact, declaration);
            if (declaration.kind == frontend::DeclKind::Import) {
                if (mode == DocsMode::Interface && !declaration.import.isExport)
                    continue;
                module.symbols.push_back(std::move(symbol));
                if (!declaration.import.isExport)
                    continue;

                const auto *edge = edgeForImport(snapshot, graphModule.artifact->key, declaration);
                if (edge == nullptr || edge->targetKind != session::ImportTargetKind::Zith)
                    continue;
                for (const auto &target : edge->targets) {
                    const auto targetSymbols = allSymbols.find(target);
                    if (targetSymbols == allSymbols.end())
                        continue;
                    for (auto reexport : targetSymbols->second) {
                        if (reexport.kind == frontend::DeclKind::Import ||
                            reexport.visibility != frontend::Visibility::Public)
                            continue;
                        const auto exportedName = reexportedName(declaration.import, reexport.name);
                        if (exportedName.empty())
                            continue;
                        reexport.name       = exportedName;
                        reexport.visibility = frontend::Visibility::Public;
                        reexport.isReexport = true;
                        if (!hasKind(module, reexport))
                            module.symbols.push_back(std::move(reexport));
                    }
                }
                continue;
            }

            const bool include =
                mode == DocsMode::Interface
                    ? symbol.visibility == frontend::Visibility::Public
                    : (!graphModule.isDependency ||
                       declarationIsUsed(*graphModule.artifact, declaration, usedSymbols));
            if (include)
                module.symbols.push_back(std::move(symbol));
        }

        for (auto &symbol : module.symbols) {
            if (symbol.members.empty())
                continue;
            std::erase_if(symbol.members, [&](const DocMember &member) {
                return mode == DocsMode::Interface &&
                       member.visibility != frontend::Visibility::Public;
            });
        }
        std::stable_sort(module.symbols.begin(), module.symbols.end(),
                         [](const DocSymbol &left, const DocSymbol &right) {
                             return std::tie(left.name, left.ownerName, left.signature,
                                             left.span.start, left.isReexport) <
                                    std::tie(right.name, right.ownerName, right.signature,
                                             right.span.start, right.isReexport);
                         });
        result.modules.push_back(std::move(module));
    }
    return result;
}

} // namespace zith::cli::docs
