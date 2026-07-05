#include "compilation-session.hpp"
#include "ast/ast-printer.hpp"
#include "cli/terminal.hpp"
#include "comptime/solver.hpp"
#include "diagnostics/error-codes.hpp"
#include "formatter/fmt-visitor.hpp"
#include "lexer/lexer.hpp"
#include "memory/source-map.hpp"
#include "parser/parser.hpp"
#include "sema/heuristic-engine.hpp"
#include "sema/sema-pipeline.hpp"
#include "symbols/resolver.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <toml++/toml.hpp>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif !defined(ZITH_IS_WASM)
#include <unistd.h>
#endif

namespace zith::session {

CompilationSession::CompilationSession(const Options &options, std::string filePath)
    : mOpts(options), mFilePath(std::move(filePath)), mProjectRoot(), mAstArena(), mSymArena(),
      mTypeArena(), mHirArena(), mScratchArena(), mDiags(mScratchArena),
      mInterner(std::make_unique<memory::StringInterner>(mAstArena)),
      mAstBuilder(mAstArena, *mInterner), mImportMgr(mSymArena, *mInterner, mSourceMap, mDiags),
      mSyms(mSymArena, mInterner.get()),
      mResolvedSyms(mSymArena), mTypes(mTypeArena, *mInterner), mHirModule(mHirArena),
      mProjectConfig(mAstArena) {
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
        } catch (...) {}
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
        }
#endif
    }
#endif
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

    // Stage 1: Lex
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!lexStage())
        return false;
    mPlan.advance();

    // Stage 2: Scan (register symbols, create UnbodyNodes)
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!scanStage())
        return false;
    mPlan.advance();

    // Stage 3: Import resolution
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!importStage())
        return false;
    mPlan.advance();

    // Stage 4: Name resolution
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!resolveStage())
        return false;
    mPlan.advance();

    // Stage 5: Type checking + body expansion + HIR lowering
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!semaStage())
        return false;
    mPlan.advance();

    // Stage 6: Solver (generic instantiation, monomorphization)
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!solveStage())
        return false;
    mPlan.advance();

    // Stage 7: NRA (Node Resource Analysis — ownership/borrowing)
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!nraStage())
        return false;
    mPlan.advance();

    // Stage 8: Codegen (LLVM IR emission)
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!codegenStage())
        return false;
    mPlan.advance();

    // Stage 9: Cache output
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

    if (mOpts.get().flags.verbose()) {
        std::error_code ec;
        auto fsize = fs::file_size(mFilePath, ec);
        if (ec)
            writeOutput("[file] %s\n", mFilePath.c_str());
        else
            writeOutput("[file] %s (%.1f KiB)\n", mFilePath.c_str(), fsize / 1024.0);
    }
#endif

    auto file_result = !mContentOverride.empty()
                           ? mSourceMap.addFile(mFilePath, mContentOverride)
                           : mSourceMap.loadFile(mFilePath);
    if (!file_result) {
        writeOutput("%s[error]%s failed to load file '%s'\n", ansicolor("\033[31m"),
                    ansicolor("\033[0m"), mFilePath.c_str());
        return false;
    }
    mFileId = file_result.value();

    auto token_result = lexer::tokenize(mSourceMap, mScratchArena, mFileId, mDiags);
    if (!token_result) {
        mDiags.emit();
        return false;
    }
    mTokens = token_result.value();

    if (mOpts.get().flags.printTokens() || mOpts.get().flags.emitTokens())
        lexer::printTokens(mTokens);

    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [lex] %6u tokens  (%5.1fms)\n", mTokens.len, dt);
    }

    return true;
}

bool CompilationSession::importStage() {
    auto t0      = std::chrono::steady_clock::now();
    namespace fs = std::filesystem;

    // ── Compute visible roots ──────────────────────────────────────
    std::vector<std::string> visible_roots;

    // 1. Stdlib path (relative to compiler executable)
#ifndef ZITH_IS_WASM
    {
        fs::path exe_dir;
        char exe_buf[4096];
#ifdef _WIN32
        DWORD len = GetModuleFileNameA(NULL, exe_buf, sizeof(exe_buf));
        if (len != 0 && len != sizeof(exe_buf)) {
            exe_buf[len] = '\0';
            exe_dir      = fs::path(exe_buf).parent_path();
        }
#elif defined(__APPLE__)
        uint32_t size = sizeof(exe_buf);
        if (_NSGetExecutablePath(exe_buf, &size) == 0) {
            exe_dir = fs::path(exe_buf).parent_path();
        }
#else
        ssize_t exe_len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (exe_len != -1) {
            exe_buf[exe_len] = '\0';
            exe_dir          = fs::path(exe_buf).parent_path();
        }
#endif
        if (!exe_dir.empty()) {
            auto candidates = {
                exe_dir.parent_path() / "stdlib",
                exe_dir / "stdlib",
                exe_dir.parent_path() / "share" / "zith" / "stdlib",
            };
            for (auto &c : candidates) {
                if (fs::is_directory(c)) {
                    visible_roots.push_back(fs::weakly_canonical(c).string());
                    break;
                }
            }
        }
    }
#endif

    // 2. -I include dirs from CLI
    for (auto &d : mOpts.get().includeDirs)
        visible_roots.push_back(fs::weakly_canonical(fs::path(d)).string());

    // 3. Project root
    if (!mProjectRoot.empty())
        visible_roots.push_back(mProjectRoot);

    // 4. src_dirs from ZithProject.toml [paths] (list)
    for (const auto &sd : mProjectConfig.srcDirs) {
        auto p = fs::weakly_canonical(fs::path(mProjectRoot) / sd);
        visible_roots.push_back(p.string());
    }
    // 5. mod_dir, test_dir from ZithProject.toml [paths]
    if (!mProjectConfig.modDir.empty()) {
        auto p = fs::weakly_canonical(fs::path(mProjectRoot) / mProjectConfig.modDir);
        visible_roots.push_back(p.string());
    }
    if (!mProjectConfig.testDir.empty()) {
        auto p = fs::weakly_canonical(fs::path(mProjectRoot) / mProjectConfig.testDir);
        visible_roots.push_back(p.string());
    }

    mImportMgr.setVisibleRoots(std::move(visible_roots));

    auto source_dir = fs::path(mFilePath).parent_path().string();

    for (auto decl_id : mProgram.decls) {
        auto &decl = mAstBuilder.getDecl(decl_id);
        if (auto *import = std::get_if<ast::ImportNode>(&decl)) {
            auto res = mImportMgr.resolve(import->path, import->is_from, import->is_export,
                                            import->alias, import->import_depth, source_dir);
            if (!res) {
                mDiags.report(diagnostics::Severity::Error, diagnostics::err::ImportError,
                              std::string(res.error().msg), import->span);
                continue;
            }
        }
    }

    mImportMgr.mergeInto(mSyms);

    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [import] %zu symbols  (%5.1fms)\n", mSyms.symbolCount(), dt);
    }

    return true;
}

bool CompilationSession::resolveStage() {
    auto t0 = std::chrono::steady_clock::now();

    symbols::Resolver resolver(mSyms, mImportMgr, mAstBuilder, mDiags);
    resolver.resolveProgram(mProgram);
    mResolvedSyms = resolver.takeResolvedTable();

    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [resolve] %5.1fms\n", dt);
    }

    return !mDiags.hasErrors();
}

bool CompilationSession::scanStage() {
    auto t0 = std::chrono::steady_clock::now();
    parser::Parser parser(&mTokens, &mAstBuilder, &mDiags);
    mScanResult = parser::scan(parser, mSyms);
    mProgram    = std::move(parser.program);
    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }
    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [scan] %6zu top-level decls  (%5.1fms)\n", mProgram.decls.size(), dt);
    }
    return true;
}

bool CompilationSession::semaStage() {
    auto t0 = std::chrono::steady_clock::now();

    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }

    // Expand unbody nodes into real AST before type-checking
    {
        parser::Parser parser(&mTokens, &mAstBuilder, &mDiags);
        parser.program = std::move(mProgram);
        parser.expandBodies(mScanResult);
        mProgram = std::move(parser.program);
    }

    if (mOpts.get().flags.emitAst()) {
        std::printf("--- AST ---\n");
        ast::printAST(mProgram, mAstBuilder);
        std::printf("--- Symbols ---\n");
        mSyms.dump();
        std::printf("---\n");
    }

    sema::SemaPipeline pipeline(mSyms, mTypes, mDiags, mAstBuilder, mHirArena, &mResolvedSyms);
    if (!pipeline.run(mProgram)) {
        mDiags.emit();
        return false;
    }

    mHirModule = pipeline.takeHir();

    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [sema] %zu fns lowered  (%5.1fms)\n", mHirModule.getFnCount(), dt);
    }

    return !mDiags.hasErrors();
}

bool CompilationSession::solveStage() {
    auto t0 = std::chrono::steady_clock::now();
    comptime::Solver solver(mTypes, mAstBuilder, mProgram, mSyms, mDiags, mHirArena);
    if (!solver.solve(mHirModule)) {
        mDiags.emit();
        return false;
    }
    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [solve] \xe2\x80\x94 (stub)  (%5.1fms)\n", dt);
    }
    return true;
}

bool CompilationSession::nraStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [nra] \xe2\x80\x94 (stub)  (%5.1fms)\n", dt);
    }
    return true;
}

bool CompilationSession::codegenStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [codegen] \xe2\x80\x94 (stub)  (%5.1fms)\n", dt);
    }
    return true;
}

bool CompilationSession::cacheStage() {
    auto t0 = std::chrono::steady_clock::now();
    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [cache] \xe2\x80\x94 (stub)  (%5.1fms)\n", dt);
    }
    return true;
}

std::string CompilationSession::fmtStage() {
    if (!lexStage())
        return {};
    if (!scanStage())
        return {};

    {
        parser::Parser parser(&mTokens, &mAstBuilder, &mDiags);
        parser.program = std::move(mProgram);
        parser.expandBodies(mScanResult);
        mProgram = std::move(parser.program);
    }

    formatter::FmtVisitor visitor(mAstBuilder, mProgram);
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
            mOutputBuffer.resize(old + len);
            std::vsnprintf(mOutputBuffer.data() + old, len + 1, fmt, args);
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
