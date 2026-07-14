#include "compilation-session.hpp"
#include "ast/ast-printer.hpp"
#include "cli/terminal.hpp"
#ifdef ZITH_HAS_LLVM
#include "codegen/codegen.hpp"
#include <llvm/TargetParser/Host.h>
#endif
#include "comptime/solver.hpp"
#include "diagnostics/error-codes.hpp"
#include "formatter/fmt-visitor.hpp"
#include "lexer/lexer.hpp"
#include "memory/source-map.hpp"
#include "parser/parser.hpp"
#include "sema/heuristic-engine.hpp"
#include "sema/hir-lower.hpp"
#include "sema/sema-pipeline.hpp"
#include "symbols/resolver.hpp"
#include "types/type-lower.hpp"

#ifdef ZITH_HAS_LLVM
#include "codegen/codegen.hpp"
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <toml++/toml.hpp>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/wait.h>
#elif !defined(ZITH_IS_WASM)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace zith::session {

CompilationSession::CompilationSession(const Options &options, std::string filePath)
    : mOpts(options), mFilePath(std::move(filePath)), mProjectRoot(), mProjectConfig(mAstArena),
      mScratchArena(), mAstArena(), mSymArena(), mTypeArena(), mHirArena(), mDiags(mScratchArena),
      mInterner(std::make_unique<memory::StringInterner>(mAstArena)),
      mAstBuilder(mAstArena, *mInterner), mImportMgr(mSymArena, *mInterner, mSourceMap, mDiags),
      mSyms(mSymArena, mInterner.get()), mResolvedSyms(mSymArena), mTypes(mTypeArena, *mInterner),
      mHirModule(mHirArena), mTypedAst(mHirArena) {
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

    // Stage 5: Type checking + body expansion
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!semaStage())
        return false;
    mPlan.advance();

    // Stage 6: HIR lowering
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!lowerStage())
        return false;
    mPlan.advance();

    // Stage 7: Solver (generic instantiation, monomorphization)
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!solveStage())
        return false;
    mPlan.advance();

    // Stage 8: NRA (Node Resource Analysis — ownership/borrowing)
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!nraStage())
        return false;
    mPlan.advance();

    // Stage 9: Codegen (LLVM IR emission)
    if (mPlan.shouldStop())
        return !mDiags.hasErrors();
    if (!codegenStage())
        return false;
    mPlan.advance();

    // Stage 10: Cache output
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
            writeOutput("[file] %s (%.1f KiB)\n", mFilePath.c_str(),
                        static_cast<double>(fsize) / 1024.0);
    }
#endif

    auto file_result = !mContentOverride.empty() ? mSourceMap.addFile(mFilePath, mContentOverride)
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

    if (mOpts.get().flags.printTokens() || mOpts.get().flags.emitTokens()) {
        std::fputs("--- Tokens ---\n", stdout);
        lexer::printTokens(mTokens);
        std::fputs("---\n", stdout);
    }

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
        DWORD len = GetModuleFileNameA(nullptr, exe_buf, sizeof(exe_buf));
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

    // Asset roots
    std::vector<std::string> asset_roots;
    for (auto &d : mOpts.get().assetDirs)
        asset_roots.push_back(fs::weakly_canonical(fs::path(d)).string());
    if (!mProjectConfig.assetDir.empty()) {
        auto p = fs::weakly_canonical(fs::path(mProjectRoot) / mProjectConfig.assetDir);
        asset_roots.push_back(p.string());
    }
    if (!mProjectRoot.empty()) {
        auto default_assets = fs::weakly_canonical(fs::path(mProjectRoot) / "assets");
        if (fs::is_directory(default_assets))
            asset_roots.push_back(default_assets.string());
    }
    mImportMgr.setAssetRoots(std::move(asset_roots));

    auto source_dir = fs::path(mFilePath).parent_path().string();

    memory::DynArray<size_t> root_deps(mSymArena);
    for (auto decl_id : mProgram.decls) {
        auto &decl = mAstBuilder.getDecl(decl_id);
        if (auto *import = std::get_if<ast::ImportNode>(&decl)) {
            auto res = mImportMgr.resolve(import->path, import->symbols, import->is_from,
                                          import->is_export, import->is_asset, import->alias,
                                          import->import_depth, source_dir);
            if (!res) {
                mDiags.report(diagnostics::Severity::Error, diagnostics::err::ImportError,
                              std::string(res.error().msg), import->span);
                continue;
            }
            root_deps.push(res.value());
        }
    }

    mImportMgr.setRootDeps(std::move(root_deps));
    mImportMgr.mergeInto(mSyms);

    // Imported declarations are parsed with independent AST builders. Register
    // nominal types before aliases so aliases may refer to types in any loaded
    // file.
    for (size_t fi = 0; fi < mImportMgr.fileCount(); ++fi) {
        const auto &file = mImportMgr.get(fi);
        if (!file.builder)
            continue;
        for (auto decl_id : file.program.decls) {
            const auto &decl = file.builder->getDecl(decl_id);
            if (auto *sd = std::get_if<ast::StructDeclNode>(&decl))
                mTypes.registerNamedType(sd->name, types::TypeKind::Struct);
            else if (auto *ed = std::get_if<ast::EnumDeclNode>(&decl))
                mTypes.registerNamedType(ed->name, types::TypeKind::Enum);
            else if (auto *ud = std::get_if<ast::UnionDeclNode>(&decl))
                mTypes.registerNamedType(ud->name, types::TypeKind::Union);
        }
    }
    for (size_t fi = 0; fi < mImportMgr.fileCount(); ++fi) {
        const auto &file = mImportMgr.get(fi);
        if (!file.builder)
            continue;
        for (auto decl_id : file.program.decls) {
            const auto &decl = file.builder->getDecl(decl_id);
            if (auto *ad = std::get_if<ast::TypeAliasDeclNode>(&decl)) {
                types::TypeLower lower(*file.builder, mTypes, mDiags, mSyms);
                mTypes.registerTypeAlias(ad->name, lower.lower(ad->target_type));
            }
        }
    }

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
        mSyms.dump(stdout, &mAstBuilder);
        std::printf("---\n");
    }

    sema::SemaPipeline pipeline(mSyms, mTypes, mDiags, mAstBuilder, mHirArena, &mResolvedSyms,
                                &mImportMgr);
    if (!pipeline.run(mProgram)) {
        mDiags.emit();
        return false;
    }

    mTypedAst = pipeline.takeTypedAst();

    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [sema] typed AST ready  (%5.1fms)\n", dt);
    }

    return !mDiags.hasErrors();
}

bool CompilationSession::lowerStage() {
    auto t0 = std::chrono::steady_clock::now();

    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }

    sema::HirLower lower(mSyms, mTypes, mDiags, mAstBuilder, mHirArena, mTypedAst, &mResolvedSyms,
                         &mImportMgr);
    if (!lower.run(mProgram)) {
        mDiags.emit();
        return false;
    }

    mHirModule = lower.takeHir();

    if (mOpts.get().flags.emitHir()) {
        std::fputs("--- HIR ---\n", stdout);
        mHirModule.dump(stdout, *mInterner);
        std::fputs("---\n", stdout);
    }

    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [lower] %zu fns lowered  (%5.1fms)\n", mHirModule.getFnCount(), dt);
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

    if (mDiags.hasErrors()) {
        mDiags.emit();
        return false;
    }

#ifdef ZITH_HAS_LLVM
    {
        codegen::CodeGen cg(*mInterner, mSyms, mTypes, mAstBuilder, mOpts.get().targetTriple,
                            mOpts.get().flags.optLevel(), &mDiags);
        cg.emit(mHirModule, mFilePath);
        cg.optimize();

        if (mOpts.get().flags.emitIr()) {
            auto ir = cg.printIR();
            writeOutput("%s\n", ir.c_str());
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
            std::string objPath = mOpts.get().outputFile;
            if (objPath.empty()) {
                namespace fs       = std::filesystem;
                std::string objDir = (fs::path(mProjectRoot) / "cache").string();
                fs::create_directories(objDir);
                objPath = objDir + "/" + fs::path(mFilePath).filename().string() + ".o";
            }
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

    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [codegen] %5.1fms\n", dt);
    }

    return !mDiags.hasErrors();
}

bool CompilationSession::cacheStage() {
#ifndef ZITH_IS_WASM
    auto t0      = std::chrono::steady_clock::now();
    namespace fs = std::filesystem;
    fs::create_directories(fs::path(mProjectRoot) / "cache");
    if (mOpts.get().flags.verbose()) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [cache] \xe2\x80\x94 ready  (%5.1fms)\n", dt);
    }
#endif
    return true;
}

bool CompilationSession::linkAndExec() {
#ifndef ZITH_IS_WASM
    if (mObjectPath.empty())
        return false;

    // Determine executable path
    std::string exePath;
    if (!mOpts.get().outputFile.empty() && mOpts.get().outputFile != mObjectPath) {
        exePath = mOpts.get().outputFile;
    } else {
        namespace fs = std::filesystem;
        // Determine binary name
        std::string binName;
        if (!mProjectConfig.name.empty())
            binName = mProjectConfig.name;
        else
            binName = fs::path(mFilePath).stem().string();

        // Compute output directory
        std::string exeDir = (fs::path(mProjectRoot) / "target").string();
        // Cross-compilation: target/<triple>/ subdirectory
        auto &triple = mOpts.get().targetTriple;
        if (!triple.empty()) {
#ifdef ZITH_HAS_LLVM
            std::string hostTriple = llvm::sys::getDefaultTargetTriple();
#else
            std::string hostTriple;
#endif
            if (triple != hostTriple)
                exeDir = (fs::path(mProjectRoot) / "target" / triple).string();
        }

        fs::create_directories(exeDir);
        exePath = exeDir + "/" + binName;
    }

    auto &triple = mOpts.get().targetTriple;
    bool isWasm = triple.find("wasm32") != std::string::npos || triple.find("wasm64") != std::string::npos;

    if (isWasm && !mOpts.get().outputFile.empty() && mOpts.get().outputFile != mObjectPath) {
        exePath = mOpts.get().outputFile;
    } else if (isWasm) {
        exePath += ".wasm";
    } else {
#ifdef _WIN32
        if (mOpts.get().outputFile.empty() || mOpts.get().outputFile == mObjectPath)
            exePath += ".exe";
#endif
    }

    if (mOpts.get().flags.verbose())
        writeOutput("  [link] %s -> %s\n", mObjectPath.c_str(), exePath.c_str());

    std::string linkCmd;
    if (isWasm) {
        bool isWasi = triple.find("wasi") != std::string::npos;
        linkCmd = "wasm-ld ";
        if (!isWasi)
            linkCmd += "--no-entry --export-all ";
        linkCmd += "-o " + exePath + " " + mObjectPath;
    } else {
        linkCmd = "/usr/bin/cc -o " + exePath + " " + mObjectPath;
    }
    
    if (!mOpts.get().sysroot.empty())
        linkCmd += " --sysroot=" + mOpts.get().sysroot;
    
    if (mOpts.get().flags.verbose())
        writeOutput("  [link] %s\n", linkCmd.c_str());

    int linkResult = std::system(linkCmd.c_str());
    if (linkResult != 0) {
        writeOutput("%s[error]%s linking failed (exit code %d)\n", ansicolor("\033[31m"),
                    ansicolor("\033[0m"), linkResult);
        return false;
    }

    if (mOpts.get().flags.verbose())
        writeOutput("  [exec] %s\n", exePath.c_str());

    if (isWasm) {
        if (mOpts.get().flags.verbose())
            writeOutput("  [exec] skipped (cannot natively execute WASM)\n");
        mChildExitCode = 0;
        return true;
    }

    // Execute the binary and propagate its exit code
    int execResult = std::system(exePath.c_str());
    if (execResult == -1) {
        writeOutput("%s[error]%s failed to launch executable\n", ansicolor("\033[31m"),
                    ansicolor("\033[0m"));
        return false;
    }

#ifdef _WIN32
    mChildExitCode = execResult;
#else
    if (WIFEXITED(execResult)) {
        mChildExitCode = WEXITSTATUS(execResult);
    } else if (WIFSIGNALED(execResult)) {
        mChildExitCode = 128 + WTERMSIG(execResult);
    } else {
        mChildExitCode = 1;
    }
#endif

    return true;
#else
    (void)mObjectPath;
    writeOutput("%s[error]%s cannot execute on WASM target\n", ansicolor("\033[31m"),
                ansicolor("\033[0m"));
    return false;
#endif
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
