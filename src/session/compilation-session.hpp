#pragma once

#include "ast/ast-builder.hpp"
#include "cli/options.hpp"
#include "cli/project-config.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"
#include "lexer/token.hpp"
#include "memory/source-map.hpp"
#include "memory/string-interner.hpp"
#include "parser/scan-result.hpp"
#include "session/pipeline-plan.hpp"
#include "symbols/import-manager.hpp"
#include "symbols/symbol-table.hpp"
#include "types/type-intern.hpp"

#include <cstdarg>
#include <functional>
#include <memory>
#include <string>

namespace zith::session {

class CompilationSession {
    std::reference_wrapper<const Options> mOpts;
    std::string mFilePath;
    std::string mProjectRoot;
    std::string mObjectPath;
    int mChildExitCode = 0;
    ProjectConfig mProjectConfig;
    PipelinePlan mPlan;

    memory::SourceMap mSourceMap;
    memory::Arena mScratchArena;
    memory::Arena mAstArena;
    memory::Arena mSymArena;
    memory::Arena mTypeArena;
    memory::Arena mHirArena;
    diagnostics::DiagnosticEngine mDiags;

    std::unique_ptr<memory::StringInterner> mInterner;
    ast::AstBuilder mAstBuilder;
    symbols::ImportManager mImportMgr;
    symbols::SymbolTable mSyms;
    memory::DynArray<symbols::SymId> mResolvedSyms;
    types::TypeIntern mTypes;
    hir::HirModule mHirModule;

    memory::FileId mFileId = 0;
    lexer::TokenStream mTokens{};
    ast::ProgramNode mProgram{mAstArena};
    parser::ScanResult mScanResult{mAstArena};

    std::string mOutputBuffer;
    bool mBufferedOutput   = false;
    bool mAlwaysEmitObject = false;
    std::string mContentOverride;

#if defined(__GNUC__) || defined(__clang__)
    void writeOutput(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
    void writeOutput(const char *fmt, ...);
#endif

    const char *ansicolor(const char *code) const {
        return mDiags.useColor() ? code : "";
    }

public:
    CompilationSession(const Options &opts, std::string filePath);

    CompilationSession(CompilationSession &&)                 = default;
    CompilationSession &operator=(CompilationSession &&)      = delete;
    CompilationSession(const CompilationSession &)            = delete;
    CompilationSession &operator=(const CompilationSession &) = delete;

    bool run();
    bool runTo(Stage target);

    const diagnostics::DiagnosticEngine &diags() const {
        return mDiags;
    }
    diagnostics::DiagnosticEngine &diags() {
        return mDiags;
    }
    const std::string &filePath() const {
        return mFilePath;
    }
    bool hasErrors() const {
        return mDiags.hasErrors();
    }
    const ProjectConfig &projectConfig() const {
        return mProjectConfig;
    }
    const memory::SourceMap &sourceMap() const {
        return mSourceMap;
    }
    memory::FileId fileId() const {
        return mFileId;
    }

    void setBuffered(bool b) {
        mBufferedOutput = b;
        mDiags.setSuppressEmit(b);
    }
    void setContent(std::string content) {
        mContentOverride = std::move(content);
    }
    void setAlwaysEmitObject(bool v) {
        mAlwaysEmitObject = v;
    }
    std::string flushOutput();
    void emitDiagnostics();
    bool linkAndExec();
    int childExitCode() const {
        return mChildExitCode;
    }

    const lexer::TokenStream &tokens() const {
        return mTokens;
    }
    const parser::ScanResult &scanResult() const {
        return mScanResult;
    }
    const symbols::SymbolTable &symbolTable() const {
        return mSyms;
    }
    symbols::SymbolTable &symbolTable() {
        return mSyms;
    }
    const memory::DynArray<symbols::SymId> &resolvedSyms() const {
        return mResolvedSyms;
    }
    const ast::AstBuilder &astBuilder() const {
        return mAstBuilder;
    }
    ast::AstBuilder &astBuilder() {
        return mAstBuilder;
    }
    const ast::ProgramNode &program() const {
        return mProgram;
    }
    const types::TypeIntern &types() const {
        return mTypes;
    }
    memory::StringInterner &interner() {
        return *mInterner;
    }

    std::string fmtStage();

private:
    void setTarget(Stage s) {
        mPlan.target = s;
    }
    bool lexStage();
    bool scanStage();
    bool importStage();
    bool resolveStage();
    bool semaStage();
    bool solveStage();
    bool nraStage();
    bool codegenStage();
    bool cacheStage();
};

} // namespace zith::session
