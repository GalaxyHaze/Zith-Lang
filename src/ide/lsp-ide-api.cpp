#include "ide/lsp-ide-api.hpp"

#include "cli/options.hpp"
#include "diagnostics/diagnostic.hpp"
#include "frontend/frontend.hpp"
#include "memory/source-file.hpp"
#include "memory/arena.hpp"
#include "session/compilation-session.hpp"
#include "session/frontend-context.hpp"
#include "types/type-kind.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>

namespace zith::ide {

namespace {

Range rangeFromSource(const std::string &source, memory::FileId file_id,
                      uint32_t start, uint32_t end) {
    Range result{};
    uint32_t line = 0;
    uint32_t column = 0;
    const uint32_t source_size = static_cast<uint32_t>(source.size());
    for (uint32_t i = 0; i < start && i < source_size; ++i) {
        if (source[i] == '\n') {
            ++line;
            column = 0;
        } else {
            ++column;
        }
    }
    result.start.line = line;
    result.start.character = column;
    for (uint32_t i = start; i < end && i < source_size; ++i) {
        if (source[i] == '\n') {
            ++line;
            column = 0;
        } else {
            ++column;
        }
    }
    result.end.line = line;
    result.end.character = column;
    (void)file_id;
    return result;
}

Severity toIdeSeverity(diagnostics::Severity severity) {
    switch (severity) {
    case diagnostics::Severity::Note:
        return Severity::Note;
    case diagnostics::Severity::Warning:
        return Severity::Warning;
    case diagnostics::Severity::Bug:
        return Severity::Bug;
    case diagnostics::Severity::Error:
        return Severity::Error;
    }
    return Severity::Error;
}

std::vector<Diagnostic> collectDiagnostics(const session::CompilationSession &session,
                                           std::string_view source) {
    std::vector<Diagnostic> result;
    const auto &diagnostics = session.diags().all();
    result.reserve(diagnostics.size());
    for (const auto &diag : diagnostics) {
        Diagnostic out;
        out.range = rangeFromSource(std::string(source), diag.primary.file,
                                    diag.primary.start, diag.primary.end);
        out.severity = toIdeSeverity(diag.severity);
        out.code = diag.code;
        out.message = diag.message;
        if (!diag.suggestions.empty()) {
            out.suggestions.assign(diag.suggestions.begin(), diag.suggestions.end());
        }
        result.push_back(std::move(out));
    }
    return result;
}

std::vector<DocumentSymbol> collectDocumentSymbols(const session::CompilationSession &session) {
    std::vector<DocumentSymbol> result;
    const auto &snapshot = session.snapshot();
    if (!snapshot || snapshot->modules().empty()) {
        return result;
    }
    const auto file_id = session.fileId();
    const frontend::FrontendSnapshot *frontend = nullptr;
    for (const auto &module : snapshot->modules()) {
        if (module && module->fileId == file_id && module->frontend) {
            frontend = module->frontend.get();
            break;
        }
    }
    if (!frontend) {
        return result;
    }
    const std::string &source = frontend->source();
    for (const auto &decl : frontend->declarations()) {
        DocumentSymbol symbol;
        symbol.name = decl.name.empty() ? std::string("(unnamed)") : decl.name;
        switch (decl.kind) {
        case frontend::DeclKind::Function:
            symbol.kind = 12;
            break;
        case frontend::DeclKind::Struct:
        case frontend::DeclKind::Union:
            symbol.kind = 23;
            break;
        case frontend::DeclKind::Trait:
        case frontend::DeclKind::Interface:
            symbol.kind = 11;
            break;
        case frontend::DeclKind::Enum:
            symbol.kind = 10;
            break;
        case frontend::DeclKind::TypeAlias:
            symbol.kind = 3;
            break;
        case frontend::DeclKind::Variable:
            symbol.kind = 13;
            break;
        default:
            symbol.kind = 6;
            break;
        }
        symbol.range = rangeFromSource(source, file_id, decl.span.start, decl.span.end);
        symbol.selectionRange = symbol.range;
        result.push_back(std::move(symbol));
    }
    return result;
}

std::vector<CompletionItem> collectCompletionItems(const session::CompilationSession &session) {
    std::vector<CompletionItem> result;
    const auto &snapshot = session.snapshot();
    if (!snapshot) {
        return result;
    }
    const auto file_id = session.fileId();
    for (const auto &module : snapshot->modules()) {
        if (!module || module->fileId != file_id || !module->frontend) {
            continue;
        }
        for (const auto &decl : module->frontend->declarations()) {
            if (decl.name.empty()) {
                continue;
            }
            CompletionItem item;
            item.label = decl.name;
            item.kind = decl.kind == frontend::DeclKind::Function ? 3 : 6;
            item.detail = decl.kind == frontend::DeclKind::Function ? "fn" : "declaration";
            item.insertText = decl.name;
            item.dataKind = decl.kind == frontend::DeclKind::Function
                                ? "function"
                                : (decl.kind == frontend::DeclKind::Struct ? "struct"
                                                                           : "declaration");
            item.dataName = decl.name;
            result.push_back(std::move(item));
        }
        constexpr std::string_view keywords[] = {
            "state", "dock", "jump", "in", "and", "or", "xor",
            "fn",    "let",  "var",  "if", "when", "return", "struct",
        };
        for (const auto keyword : keywords) {
            CompletionItem item;
            item.label = std::string(keyword);
            item.kind = 14;
            item.detail = "keyword";
            item.dataKind = "keyword";
            result.push_back(std::move(item));
        }
        break;
    }
    return result;
}

std::vector<SemanticToken> collectSemanticTokens(const session::CompilationSession &session) {
    std::vector<SemanticToken> result;
    const auto &snapshot = session.snapshot();
    if (!snapshot) {
        return result;
    }
    const auto file_id = session.fileId();
    for (const auto &module : snapshot->modules()) {
        if (!module || module->fileId != file_id || !module->frontend) {
            continue;
        }
        const auto &frontend = *module->frontend;
        uint32_t last_line = 0;
        uint32_t last_start = 0;
        for (const auto &token : frontend.tokens()) {
            if (token.kind == frontend::TokenKind::End) {
                break;
            }
            const auto range = rangeFromSource(frontend.source(), file_id,
                                               token.span.start, token.span.end);
            SemanticToken semantic;
            semantic.deltaLine = range.start.line - last_line;
            if (semantic.deltaLine == 0) {
                semantic.deltaStart = range.start.character - last_start;
            } else {
                semantic.deltaStart = range.start.character;
            }
            semantic.length = token.span.size();
            semantic.tokenType =
                token.kind == frontend::TokenKind::Keyword ? 7 : 0;
            semantic.tokenModifiers = 0;
            result.push_back(semantic);
            last_line = range.start.line;
            last_start = range.start.character;
        }
        break;
    }
    return result;
}

std::string sourceFromSnapshot(
    const std::shared_ptr<const session::CompilationSnapshot> &snapshot,
    session::CompilationSession &session,
    const std::string &fallback) {
    if (!snapshot) {
        return fallback;
    }
    const auto file_id = session.fileId();
    for (const auto &module : snapshot->modules()) {
        if (module && module->fileId == file_id && module->frontend) {
            return module->frontend->source();
        }
    }
    return fallback;
}

} // namespace

struct Workspace::Impl {
    explicit Impl(WorkspaceConfig config)
        : config_(std::move(config)), arena_() {
        if (!config_.compilerVersion.empty()) {
            frontend_config_.compilerVersion = config_.compilerVersion;
        }
        frontend_config_.workspaceRoot = config_.workspaceRoot;
        frontend_config_.maxFrontendWorkers =
            config_.maxWorkers == 0 ? 1 : config_.maxWorkers;
        if (!config_.stdlibPath.empty()) {
            frontend_config_.stdlibRoots.push_back(config_.stdlibPath);
        }
        frontend_context_ =
            std::make_shared<session::FrontendContext>(frontend_config_);
    }

    Options makeOptions() {
        Options opts(arena_);
        opts.targetStage = session::Stage::TypeChecked;
        opts.noCache = true;
        if (!config_.stdlibPath.empty()) {
            opts.includeDirs.push(config_.stdlibPath);
        }
        return opts;
    }

    WorkspaceConfig config_;
    session::FrontendConfig frontend_config_;
    std::shared_ptr<session::FrontendContext> frontend_context_;
    memory::Arena arena_;
    std::unordered_map<std::string, DocumentSnapshot> documents_;
};

Workspace::Workspace(WorkspaceConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

Workspace::~Workspace() = default;

DocumentSnapshot Workspace::openDocument(std::string uri, std::string filePath,
                                         std::string text, int64_t version) {
    DocumentSnapshot snapshot;
    snapshot.uri = std::move(uri);
    snapshot.filePath = std::move(filePath);
    snapshot.text = std::move(text);
    snapshot.version = version;
    snapshot.revision = 1;
    impl_->documents_[snapshot.uri] = snapshot;
    return snapshot;
}

DocumentSnapshot Workspace::changeDocument(std::string uri, std::string text,
                                           int64_t version) {
    auto existing = impl_->documents_.find(uri);
    if (existing == impl_->documents_.end()) {
        return {};
    }
    existing->second.text = std::move(text);
    existing->second.version = version;
    ++existing->second.revision;
    return existing->second;
}

void Workspace::closeDocument(const std::string &uri) noexcept {
    impl_->documents_.erase(uri);
}

std::optional<DocumentSnapshot> Workspace::snapshotFor(const std::string &uri) const {
    const auto it = impl_->documents_.find(uri);
    if (it == impl_->documents_.end()) {
        return std::nullopt;
    }
    return it->second;
}

AnalysisResult Workspace::analyze(const DocumentSnapshot &snapshot,
                                  const Query &query) {
    AnalysisResult result;
    result.kind = query.kind;
    result.analyzedRevision = snapshot.revision;

    Options opts = impl_->makeOptions();
    auto session = std::make_unique<session::CompilationSession>(
        opts, snapshot.filePath, impl_->frontend_context_);
    session->setContent(snapshot.text);
    session->setBuffered(true);
    const bool success =
        session->runTo(session::Stage::TypeChecked);
    result.ok = success || !session->hasErrors();

    const auto &compiler_snapshot = session->snapshot();
    const std::string source =
        sourceFromSnapshot(compiler_snapshot, *session, snapshot.text);

    switch (query.kind) {
    case QueryKind::Diagnostics:
        result.diagnostics = collectDiagnostics(*session, source);
        break;
    case QueryKind::Completion:
    case QueryKind::CompletionResolve:
        result.completions = collectCompletionItems(*session);
        break;
    case QueryKind::DocumentSymbols:
        result.documentSymbols = collectDocumentSymbols(*session);
        break;
    case QueryKind::SemanticTokensFull:
    case QueryKind::SemanticTokensDelta:
        result.semanticTokens = collectSemanticTokens(*session);
        break;
    case QueryKind::Hover:
        result.hover = HoverResult{};
        if (compiler_snapshot) {
            result.hover->markdown =
                "```zith\n" + source + "\n```";
        }
        break;
    case QueryKind::Highlights:
        (void)0;
        break;
    default:
        result.ok = true;
        break;
    }
    return result;
}

FeatureStability Workspace::stability(QueryKind kind) const noexcept {
    switch (kind) {
    case QueryKind::SemanticTokensDelta:
    case QueryKind::Commands:
        return FeatureStability::Experimental;
    case QueryKind::Configuration:
        return FeatureStability::Planned;
    default:
        return FeatureStability::Stable;
    }
}

} // namespace zith::ide
