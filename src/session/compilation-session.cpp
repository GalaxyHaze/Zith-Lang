#include "compilation-session.hpp"
#include "cli/terminal.hpp"
#ifdef ZITH_HAS_LLVM
#include "codegen/codegen.hpp"
#include <llvm/TargetParser/Host.h>
#endif
#include "comptime/solver.hpp"
#include "diagnostics/error-codes.hpp"
#include "formatter/fmt-visitor.hpp"
#include "memory/source-map.hpp"
#include "sema/heuristic-engine.hpp"
#include "sema/hir-lower-modern.hpp"
#include "sema/nra-facts.hpp"
#include "sema/sema-modern.hpp"
#include "types/type-kind.hpp"

#include "cache/cache-paths.hpp"
#include "common/ast-ids.hpp"
#include "support/stdlib-discovery.hpp"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <toml++/toml.hpp>
#include <vector>

namespace zith::session {

namespace {

symbols::SymKind mapFrontendDeclKind(const frontend::DeclKind kind) {
    switch (kind) {
    case frontend::DeclKind::Function:
        return symbols::SymKind::Fn;
    case frontend::DeclKind::TypeAlias:
        return symbols::SymKind::Alias;
    case frontend::DeclKind::Struct:
        return symbols::SymKind::Struct;
    case frontend::DeclKind::Enum:
        return symbols::SymKind::Enum;
    case frontend::DeclKind::Union:
        return symbols::SymKind::Union;
    case frontend::DeclKind::Trait:
        return symbols::SymKind::Trait;
    case frontend::DeclKind::Interface:
        return symbols::SymKind::Interface;
    case frontend::DeclKind::Variable:
        return symbols::SymKind::Variable;
    case frontend::DeclKind::Context:
        return symbols::SymKind::Context;
    case frontend::DeclKind::Word:
        return symbols::SymKind::Word;
    case frontend::DeclKind::Import:
        return symbols::SymKind::Module;
    case frontend::DeclKind::Macro:
    case frontend::DeclKind::Error:
        break;
    }
    return symbols::SymKind::Variable;
}

symbols::SymbolVisibility mapFrontendVisibility(const frontend::Visibility visibility) {
    switch (visibility) {
    case frontend::Visibility::Public:
        return symbols::SymbolVisibility::Public;
    case frontend::Visibility::Module:
        return symbols::SymbolVisibility::Module;
    case frontend::Visibility::Private:
        return symbols::SymbolVisibility::Private;
    }
    return symbols::SymbolVisibility::Private;
}

template <typename ArrayT>
[[maybe_unused]] void loadTomlStringArray(const toml::table &table, const char *key,
                                          ArrayT &destination) {
    if (const auto *values = table[key].as_array())
        for (const auto &value : *values)
            if (const auto text = value.value<std::string>())
                destination.push(*text);
}

// Runs a FILE*-based dump into an in-memory string so dump text can be routed
// through the session's own output band instead of the process stdout. The
// temporary file is used because the dump APIs still take a FILE*; it is
// removed as soon as the captured bytes have been copied out.
template <typename Fn> std::string captureStdioDump(Fn &&dump) {
    std::string result;

    const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path tmpPath = std::filesystem::temp_directory_path() /
                                          ("zithc-dump-" + std::to_string(uniqueSuffix) + ".tmp");

    FILE *tmp = nullptr;
#ifdef _WIN32
    if (_wfopen_s(&tmp, tmpPath.c_str(), L"wb+") != 0)
        return result;
#else
    tmp = std::fopen(tmpPath.c_str(), "wb+");
    if (tmp == nullptr)
        return result;
#endif

    dump(tmp);
    std::fflush(tmp);
    std::rewind(tmp);
    char buffer[4096];
    size_t bytes = 0;
    while ((bytes = std::fread(buffer, 1, sizeof(buffer), tmp)) > 0)
        result.append(buffer, bytes);
    std::fclose(tmp);
    std::error_code ignored;
    std::filesystem::remove(tmpPath, ignored);
    return result;
}

} // namespace

CompilationSession::CompilationSession(const Options &options, std::string filePath,
                                       std::shared_ptr<FrontendContext> frontend_context)
    : mOpts(options), mFilePath(std::move(filePath)), mProjectRoot(), mProjectConfig(mScratchArena),
      mScratchArena(), mSymArena(), mTypeArena(), mHirArena(), mDiags(mScratchArena),
      mInterner(std::make_unique<memory::StringInterner>(mScratchArena)),
      mSyms(mSymArena, mInterner.get()), mTypes(mTypeArena, *mInterner), mHirModule(mHirArena),
      mFrontendContext(std::move(frontend_context)) {
    mPlan.target = mOpts.get().targetStage;
    mDiags.setColor(term::useColor(mOpts));
    mDiags.setSourceMap(&mSourceMap);

#ifndef ZITH_IS_WASM
    namespace fs = std::filesystem;
    if (fs::is_directory(mFilePath))
        mProjectRoot = fs::weakly_canonical(fs::path(mFilePath)).string();
    else
        mProjectRoot = fs::weakly_canonical(fs::path(mFilePath).parent_path()).string();

    auto toml_path = fs::path(mProjectRoot) / "ZithProject.toml";
    if (fs::exists(toml_path)) {
#if TOML_EXCEPTIONS
        try {
            auto tbl = toml::parse_file(toml_path.string());
            if (auto *build = tbl["build"].as_table()) {
                if (auto v = build->get("entry"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.entry = *s;
                if (auto v = build->get("output"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.output = *s;
            }
            if (auto *paths = tbl["paths"].as_table()) {
                if (auto v = paths->get("bin_dir"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.binDir = *s;
            }
            if (auto *ffi = tbl["ffi"].as_table()) {
                loadTomlStringArray(*ffi, "include_dirs", mProjectConfig.includeDirs);
                loadTomlStringArray(*ffi, "c_source_dirs", mProjectConfig.cSourceDirs);
                loadTomlStringArray(*ffi, "library_dirs", mProjectConfig.libraryDirs);
                loadTomlStringArray(*ffi, "libraries", mProjectConfig.libraries);
                loadTomlStringArray(*ffi, "defines", mProjectConfig.defines);
            }
            if (auto *proj = tbl["project"].as_table()) {
                if (auto v = proj->get("name"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.name = *s;
            }
        } catch (...) {
        }
#else
        auto result = toml::parse_file(toml_path.string());
        if (result) {
            if (auto *build = result["build"].as_table()) {
                if (auto v = build->get("entry"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.entry = *s;
                if (auto v = build->get("output"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.output = *s;
            }
            if (auto *paths = result["paths"].as_table()) {
                if (auto v = paths->get("bin_dir"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.binDir = *s;
            }
            if (auto *ffi = result["ffi"].as_table()) {
                loadTomlStringArray(*ffi, "include_dirs", mProjectConfig.includeDirs);
                loadTomlStringArray(*ffi, "c_source_dirs", mProjectConfig.cSourceDirs);
                loadTomlStringArray(*ffi, "library_dirs", mProjectConfig.libraryDirs);
                loadTomlStringArray(*ffi, "libraries", mProjectConfig.libraries);
                loadTomlStringArray(*ffi, "defines", mProjectConfig.defines);
            }
            if (auto *proj = result["project"].as_table()) {
                if (auto v = proj->get("name"))
                    if (auto s = v->value<std::string>())
                        mProjectConfig.name = *s;
            }
        }
#endif
    }
#endif
}

void CompilationSession::ensureFrontendContext() {
    if (mFrontendContext)
        return;

    FrontendConfig config;
    config.maxFrontendWorkers = 1;
    config.workspaceRoot      = mProjectRoot;
#ifdef ZITH_VERSION
    config.compilerVersion = ZITH_VERSION;
#else
    config.compilerVersion = "dev";
#endif
    config.targetTriple = mOpts.get().targetTriple;
    config.parseFlags   = mOpts.get().flags.strict() ? "strict" : "";
    config.sysroot      = mOpts.get().sysroot;

    config.useSystemIncludeRoots = mOpts.get().systemIncludes;

    for (const auto &dir : mProjectConfig.includeDirs)
        config.includeRoots.push_back(
            (std::filesystem::path(mProjectRoot) / dir).lexically_normal().string());
    for (const auto &dir : mOpts.get().includeDirs)
        config.includeRoots.push_back(dir);
    for (const auto &define : mProjectConfig.defines)
        config.cDefines.push_back(define);
    for (const auto &define : mOpts.get().defines)
        config.cDefines.push_back(define);
    for (const auto &dir : mOpts.get().assetDirs)
        config.assetRoots.push_back(dir);
    if (!mProjectConfig.assetDir.empty())
        config.assetRoots.push_back(
            (std::filesystem::path(mProjectRoot) / mProjectConfig.assetDir).string());

    // Auto-discover stdlib relative to the compiler binary.
    for (auto &root : support::findStdlibRoots())
        config.stdlibRoots.push_back(std::move(root));

    mFrontendContext = std::make_shared<FrontendContext>(std::move(config));
}

bool CompilationSession::materializeFrontendSymbols() {
    if (!mSnapshot)
        return true;

    for (const auto &module : mSnapshot->modules()) {
        if (module->frontend == nullptr)
            continue;
        for (const auto &decl : module->frontend->declarations()) {
            if (decl.kind == frontend::DeclKind::Import || decl.kind == frontend::DeclKind::Error ||
                decl.kind == frontend::DeclKind::Macro || decl.name.empty()) {
                continue;
            }
            mSyms.declare(decl.name, mapFrontendVisibility(decl.visibility), 0,
                          mapFrontendDeclKind(decl.kind), ast::kInvalidDecl, {});
        }
    }
    return true;
}

bool CompilationSession::run() {
    return runTo(mPlan.target);
}

bool CompilationSession::runTo(Stage target) {
    auto t_start = std::chrono::steady_clock::now();
    mPlan.target = target;

    if (mOpts.get().flags.verbose())
        writeOutput("%s[zithc] [starting]%s %s\n", ansicolor("\033[36m"), ansicolor("\033[0m"),
                    mFilePath.c_str());

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!lexStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!scanStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!importStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!resolveStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!semaStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!solveStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!nraStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!lowerStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!codegenStage())
        return false;
    mPlan.advance();

    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!cacheStage())
        return false;

    bool ok = !mDiags.hasErrors();
    if (mOpts.get().flags.verbose()) {
        auto dt =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_start)
                .count();
        writeOutput("%s[zithc] [done]%s %s %s%s%s (%.1fms)\n", ansicolor("\033[36m"),
                    ansicolor("\033[0m"), mFilePath.c_str(),
                    ansicolor(ok ? "\033[32m" : "\033[31m"), ok ? "\xe2\x9c\x93" : "\xe2\x9c\x97",
                    ansicolor("\033[0m"), dt);
    }
    return ok;
}

bool CompilationSession::lexStage() {
    auto t0 = std::chrono::steady_clock::now();

#ifndef ZITH_IS_WASM
    namespace fs = std::filesystem;

    if (fs::is_directory(mFilePath)) {
        if (mProjectConfig.entry.empty()) {
            writeOutput("%s[error]%s no entry file in ZithProject.toml\n", ansicolor("\033[31m"),
                        ansicolor("\033[0m"));
            return false;
        }
        mFilePath = (fs::path(mProjectRoot) / mProjectConfig.entry).string();
    }
#endif

    ensureFrontendContext();
    mCanonicalPath = SourceCatalog::canonicalPath(mFilePath);

#ifndef ZITH_IS_WASM
    if (!mCacheStore && !mOpts.get().noCache) {
        const auto cache_root = (fs::path(mProjectRoot) / cache::kPersistentCacheDirName).string();
        mCacheStore =
            std::make_unique<cache::Store>(cache_root, mFrontendContext->config().cacheKey());
    }
    if (!mOpts.get().noCache)
        (void)tryLoadPersistentCache();

    if (mOpts.get().flags.verbose()) {
        std::error_code ec;
        auto fsize = fs::file_size(mFilePath, ec);
        if (ec)
            writeOutput("[file] %s\n", mFilePath.c_str());
        else
            writeOutput("[file] %s (%.1f KiB)\n", mFilePath.c_str(),
                        static_cast<double>(fsize) / 1024.0);
    }
#endif

    if (!mSnapshot) {
        auto snapshot = mContentOverride.empty()
                            ? mFrontendContext->analyzeFile(mFilePath)
                            : mFrontendContext->analyzeText(mFilePath, mContentOverride);
        if (!snapshot) {
            writeOutput("%s[error]%s failed to build frontend snapshot for '%s': %s\n",
                        ansicolor("\033[31m"), ansicolor("\033[0m"), mFilePath.c_str(),
                        snapshot.error().msg.c_str());
            return false;
        }
        mSnapshot = std::move(snapshot.value());
    }

    // Materialize all module sources into SourceMap.
    for (const auto &module : mSnapshot->modules()) {
        const auto materialized = mSourceMap.addFile(module->key, module->source->text);
        if (!materialized) {
            writeOutput("%s[error]%s failed to materialize frontend source '%s'\n",
                        ansicolor("\033[31m"), ansicolor("\033[0m"), module->key.c_str());
            return false;
        }
        mSnapshotDiagnosticFiles[module->fileId] = materialized.value();
    }
    const auto root_key = SourceCatalog::canonicalPath(mFilePath);
    const auto *root    = mSnapshot->findModule(root_key);
    if (!root) {
        writeOutput("%s[error]%s frontend snapshot has no root module '%s'\n",
                    ansicolor("\033[31m"), ansicolor("\033[0m"), mFilePath.c_str());
        return false;
    }
    const auto root_file = mSourceMap.addFile(root->key, root->source->text);
    if (!root_file) {
        writeOutput("%s[error]%s failed to materialize root source '%s'\n", ansicolor("\033[31m"),
                    ansicolor("\033[0m"), root->key.c_str());
        return false;
    }
    mFileId = root_file.value();

    // --emit-tokens: dump tokens from the modern frontend snapshot. Dumps are
    // compiler output, so they go through writeOutput() and never touch the
    // program's stdout during `zithc run`.
    if (mOpts.get().flags.emitTokens()) {
        const auto &tokens = root->frontend->tokens();
        writeOutput("--- Tokens ---\n");
        const auto &source = root->frontend->source();
        for (size_t i = 0; i < tokens.size(); ++i) {
            const auto &tok = tokens[i];
            auto text =
                std::string_view(source).substr(tok.span.start, tok.span.end - tok.span.start);
            writeOutput("[%3zu] %-12s '%.*s'\n", i,
                        tok.kind == frontend::TokenKind::Keyword       ? "keyword"
                        : tok.kind == frontend::TokenKind::Identifier  ? "identifier"
                        : tok.kind == frontend::TokenKind::Literal     ? "literal"
                        : tok.kind == frontend::TokenKind::Operator    ? "operator"
                        : tok.kind == frontend::TokenKind::Punctuation ? "punctuation"
                        : tok.kind == frontend::TokenKind::End         ? "end"
                                                                       : "unknown",
                        static_cast<int>(text.size()), text.data());
        }
        writeOutput("---\n");
    }

    // --emit-ast: dump AST from the modern frontend snapshot
    if (mOpts.get().flags.emitAst()) {
        writeOutput("--- AST ---\n");
        for (const auto &decl : root->frontend->declarations()) {
            writeOutput("decl %s (kind=%d, vis=%d, span=%u..%u%s%s)\n", decl.name.c_str(),
                        static_cast<int>(decl.kind), static_cast<int>(decl.visibility),
                        decl.span.start, decl.span.end, decl.ownerName.empty() ? "" : ", owner=",
                        decl.ownerName.empty() ? "" : decl.ownerName.c_str());
            if (decl.kind == frontend::DeclKind::Trait) {
                for (const auto &member : root->frontend->declarations()) {
                    if (member.kind != frontend::DeclKind::Function ||
                        member.ownerName != decl.name)
                        continue;
                    if (member.span.start < decl.span.start || member.span.end > decl.span.end)
                        continue;
                    writeOutput("  method %s%s%s\n", member.name.c_str(),
                                member.body ? " (default body)" : " (requirement)",
                                member.declaredType ? "" : "");
                }
            } else if (decl.kind == frontend::DeclKind::Interface) {
                std::string fields;
                for (std::size_t index = 0; index < decl.parameters.size(); ++index) {
                    if (index != 0U)
                        fields += ", ";
                    fields += decl.parameters[index].name;
                }
                writeOutput("  fields %s\n", fields.c_str());
            }
        }
        writeOutput("--- Symbols ---\n");
        writeOutput("%s",
                    captureStdioDump([this](FILE *out) { mSyms.dump(out, nullptr); }).c_str());
        writeOutput("---\n");
    }

    auto lexDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Lex)] = lexDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [lex] %5.1fms\n", lexDt);
    }

    return true;
}

bool CompilationSession::scanStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mCacheHydrated) {
        if (mHydratedEntry.has_value()) {
            const auto &art = mHydratedEntry->artifact;
            for (const auto &mapping : art.canonical_mappings) {
                const types::TypeCanonicalId canonical_id{mapping.hi, mapping.lo};
                const uint32_t expected = mCacheStore->assignCanonicalId(canonical_id);
                if (mapping.runtime_id != expected) {
                    mDiags.reportError(
                        diagnostics::err::UnsupportedSyntax,
                        "cached canonical opaque tag is unstable; invalidate the cache and "
                        "rebuild this project",
                        memory::Span{});
                    return false;
                }
            }
        }
        if (mOpts.get().flags.emitHir()) {
            const std::string hir_text =
                captureStdioDump([this](FILE *out) { mHirModule.dump(out, *mInterner); });
            writeOutput("--- HIR ---\n%s---\n", hir_text.c_str());
        }
        return true;
    }

    if (mSnapshot->hasErrors())
        forwardSnapshotDiagnostics();
    else
        forwardSnapshotDiagnostics(); // warnings always forwarded

    auto scanDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Scan)] = scanDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [scan] %zu top-level decls  (%5.1fms, %zu diagnostics)\n",
                    mSnapshot->modules().empty()
                        ? 0U
                        : mSnapshot->modules().front()->frontend->declarations().size(),
                    scanDt, mSnapshot->diagnostics().size());
    }
    const bool snapshot_failed = mSnapshot->hasErrors();
    forwardSnapshotDiagnostics();
    return !snapshot_failed;
}

bool CompilationSession::importStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mCacheHydrated)
        return true;

    const auto ok = materializeFrontendSymbols();
    auto importDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Import)] = importDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [import] %zu symbols  (%5.1fms)\n", mSyms.symbolCount(), importDt);
    }
    return ok;
}

void CompilationSession::forwardSnapshotDiagnostics() {
    if (!mSnapshot || mSnapshotDiagsForwarded)
        return;
    mSnapshotDiagsForwarded = true;
    for (const auto &diagnostic : mSnapshot->diagnostics()) {
        memory::FileId report_file = diagnostic.file;
        if (const auto *mapped = mSnapshotDiagnosticFiles.get(diagnostic.file)) {
            report_file = *mapped;
        }
        mDiags.report(diagnostic.severity, diagnostic.code, diagnostic.message,
                      memory::Span{report_file, diagnostic.start, diagnostic.end});
    }
    mDiags.emit();
}

bool CompilationSession::resolveStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mCacheHydrated)
        return true;

    if (mSnapshot->hasErrors()) {
        forwardSnapshotDiagnostics();
        return false;
    }
    auto resolveDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Resolve)] = resolveDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [resolve] %5.1fms\n", resolveDt);
    }
    return true;
}

bool CompilationSession::semaStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mCacheHydrated)
        return true;

    if (mSnapshot->hasErrors()) {
        forwardSnapshotDiagnostics();
        return false;
    }
    mModernSemaPipeline =
        std::make_unique<sema::modern::SemaPipeline>(mScratchArena, mDiags, *mSnapshot);
    mGenericInstantiations = std::make_unique<comptime::GenericInstantiationPass>(
        *mSnapshot, mModernSemaPipeline->typeTable());
    mModernSemaPipeline->setInstantiations(mGenericInstantiations.get());
    if (!mModernSemaPipeline->run()) {
        mDiags.emit();
        return false;
    }
    auto semaDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Sema)] = semaDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [sema] modern types ready  (%5.1fms)\n", semaDt);
    }
    return !mDiags.hasErrors();
}

bool CompilationSession::lowerStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mCacheHydrated)
        return true;

    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }

    sema::modern::HirLowerModern lower(mHirArena, mDiags, *mSnapshot, *mModernSemaPipeline, mTypes,
                                       *mInterner, mNraFacts.get(), mCacheStore.get());
    if (!lower.run()) {
        mDiags.emit();
        return false;
    }

    mHirModule = lower.takeHir();
    // The solver only needs to run on cold builds. Warm builds restore the HIR
    // artifact directly and the generic pass already ran before HIR lowering.

    comptime::Solver solver(mTypes, mSyms, mDiags, mHirArena);
    if (!solver.runPostLower(mHirModule)) {
        mDiags.emit();
        return false;
    }

    if (mOpts.get().flags.emitHir()) {
        const std::string hir_text =
            captureStdioDump([this](FILE *out) { mHirModule.dump(out, *mInterner); });
        writeOutput("--- HIR ---\n%s---\n", hir_text.c_str());
    }

    auto lowerDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Lower)] = lowerDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [lower] %zu fns lowered  (%5.1fms)\n", mHirModule.getFnCount(), lowerDt);
    }
    return !mDiags.hasErrors();
}

bool CompilationSession::solveStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }
    (void)mGenericInstantiations;
    auto solveDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Solve)] = solveDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [solve] %zu generic instantiation(s)  (%5.1fms)\n",
                    mGenericInstantiations != nullptr ? mGenericInstantiations->instanceCount()
                                                      : size_t{0},
                    solveDt);
    }
    return true;
}

bool CompilationSession::nraStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mCacheHydrated)
        return true;

    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }

    sema::modern::NraFacts nra(mHirArena, mDiags, *mSnapshot, *mModernSemaPipeline, *mInterner);
    if (!nra.run()) {
        mDiags.emit();
        return false;
    }
    mNraFacts = std::make_unique<sema::modern::NraFacts>(std::move(nra));

    auto nraDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Nra)] = nraDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [nra] residual facts: %zu locals, %zu calls  (%5.1fms)\n",
                    mNraFacts->localCount(), mNraFacts->callCount(), nraDt);
    }
    return true;
}

bool CompilationSession::codegenStage() {
    auto t0 = std::chrono::steady_clock::now();

    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }

#ifdef ZITH_HAS_LLVM
    {
        codegen::CodeGen cg(*mInterner, mTypes, mOpts.get().targetTriple,
                            mOpts.get().flags.optLevel(), &mDiags);
        cg.emit(mHirModule, mFilePath);
        cg.optimize();

        if (mOpts.get().flags.emitIr()) {
            auto ir = cg.printIR();
            writeOutput("%s\n", ir.c_str());
        }

        // `emit` may have reported an IR verification failure. Stop before any consumer
        // hands the module to a TargetMachine, which crashes on invalid IR.
        if (cg.hasInvalidIR() || mDiags.hasErrors()) {
            mDiags.emit();
            return false;
        }

        if (mOpts.get().flags.emitAsm()) {
            auto asm_str = cg.printAsm();
            if (asm_str.empty())
                writeOutput("%s[error]%s failed to generate assembly\n", ansicolor("\033[31m"),
                            ansicolor("\033[0m"));
            else
                writeOutput("%s\n", asm_str.c_str());
        }

        auto emitTarget = mOpts.get().emitTarget;
        if (mAlwaysEmitObject || emitTarget == Options::EmitTarget::Obj ||
            emitTarget == Options::EmitTarget::Bin) {
            // Object always goes to cache/; -o controls the final executable
            // path, not the object file.
            namespace fs       = std::filesystem;
            std::string objDir = (fs::path(mProjectRoot) / "cache").string();
            fs::create_directories(objDir);
            std::string objPath = objDir + "/" + fs::path(mFilePath).filename().string() + ".o";
            if (!cg.emitObject(objPath)) {
                writeOutput("%s[error]%s failed to emit object file\n", ansicolor("\033[31m"),
                            ansicolor("\033[0m"));
                return false;
            }
            mObjectPath = objPath;
        }
    }
#else
    if (mOpts.get().flags.emitIr() || mOpts.get().flags.emitAsm() ||
        mOpts.get().emitTarget == Options::EmitTarget::Obj ||
        mOpts.get().emitTarget == Options::EmitTarget::Bin) {
        writeOutput("%s[error]%s LLVM not available in this build\n", ansicolor("\033[31m"),
                    ansicolor("\033[0m"));
        return false;
    }
#endif

    auto codegenDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Codegen)] = codegenDt;
    if (mOpts.get().flags.verbose()) {
        writeOutput("  [codegen] %5.1fms\n", codegenDt);
    }
    return !mDiags.hasErrors();
}

bool CompilationSession::cacheStage() {
#ifndef ZITH_IS_WASM
    auto t0      = std::chrono::steady_clock::now();
    namespace fs = std::filesystem;
    fs::create_directories(fs::path(mProjectRoot) / cache::kPersistentCacheDirName);
    writePersistentCache();
    auto cacheDt =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    mStageDurations[static_cast<size_t>(StageIndex::Cache)] = cacheDt;
    if (mOpts.get().flags.verbose()) {
        const auto m = mCacheStore ? mCacheStore->metrics() : cache::StoreMetrics{};
        writeOutput("  [cache] hits=%zu misses=%zu writes=%zu invalid=%zu  (%5.1fms)\n", m.hits,
                    m.misses, m.writes, m.invalid, cacheDt);
    }
#endif
    return true;
}

std::unordered_map<std::string, double> CompilationSession::getStageDurationsMs() const {
    static constexpr const char *names[] = {
        "lex", "scan", "import", "resolve", "sema", "lower", "solve", "nra", "codegen", "cache",
    };
    std::unordered_map<std::string, double> result;
    for (size_t i = 0; i < static_cast<size_t>(StageIndex::Count); ++i)
        result[names[i]] = mStageDurations[i];
    return result;
}

ArenaMemoryUsage CompilationSession::getArenaMemoryUsage() const {
    return {
        mScratchArena.allocatedBytes(),
        mSymArena.allocatedBytes(),
        mTypeArena.allocatedBytes(),
        mHirArena.allocatedBytes(),
    };
}

std::string CompilationSession::fmtStage() {
    if (!lexStage())
        return {};
    if (!scanStage())
        return {};

    const auto root_key = SourceCatalog::canonicalPath(mFilePath);
    const auto *root    = mSnapshot->findModule(root_key);
    if (root == nullptr || root->frontend == nullptr) {
        writeOutput("%s[error]%s formatter could not find root frontend module '%s'\n",
                    ansicolor("\033[31m"), ansicolor("\033[0m"), mFilePath.c_str());
        return {};
    }

    formatter::FmtVisitor visitor(*root->frontend);
    visitor.format();
    return visitor.result();
}

void CompilationSession::writeOutput(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (mBufferedOutput) {
        va_list args_copy;
        va_copy(args_copy, args);
        int len = std::vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);
        if (len > 0) {
            auto old = mOutputBuffer.size();
            mOutputBuffer.resize(old + static_cast<size_t>(len));
            std::vsnprintf(mOutputBuffer.data() + old, static_cast<size_t>(len) + 1, fmt, args);
        }
    } else {
        std::vfprintf(stderr, fmt, args);
    }
    va_end(args);
}

std::string CompilationSession::flushOutput() {
    auto result = std::move(mOutputBuffer);
    mOutputBuffer.clear();
    return result;
}

std::string CompilationSession::takeChildOutput() {
    auto result = std::move(mChildOutput);
    mChildOutput.clear();
    return result;
}

void CompilationSession::emitDiagnostics() {
    sema::HeuristicEngine heuristic;
    auto &all = mDiags.diagnostics();
    for (size_t i = 0; i < all.size(); i++) {
        heuristic.generate(all[i], mSyms, all[i].suggestions);
    }
    mDiags.setSuppressEmit(false);
    mDiags.emit();
}

} // namespace zith::session
