#pragma once

#include "ast/ast-builder.hpp"
#include "cli/options.hpp"
#include "cli/pipeline-plan.hpp"
#include "cli/project-config.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "import/import-manager.hpp"
#include "import/symbol-table.hpp"
#include "lexer/token.hpp"
#include "memory/source-map.hpp"
#include "parser/scan-result.hpp"
#include "types/type-intern.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/mir/mir-module.hpp"
#include "zir/zir/zir-inst.hpp"

#include <cstdarg>
#include <string>

namespace zith::cli {

// Per-file compilation pipeline.
//
// Each CompilationSession owns its own diagnostic engine, arenas,
// AST builder, symbol table, and intermediate representations.
// This design is naturally thread-safe: files don't share mutable state.
//
// Thread-safety plan for multi-file parallel compilation:
//   - Each file gets its own CompilationSession (independent state)
//   - All arenas are owned per-session, no global thread_local state
//   - SourceMap uses internal shared_mutex for concurrent reads
//   - Stages run independently per file — no shared mutable state
//   - After MIR lowering, results CAN be merged per-module or per-binary
//     (this is the only potential serialization point)
//
// Usage:
//   CompilationSession session(opts, "foo.zith");
//   session.run();
//   if (session.hasErrors()) { ... }
class CompilationSession {
    const Options &opts_;
    std::string file_path_;
    std::string project_root_;
    ProjectConfig project_config_;
    PipelinePlan plan_;

    memory::SourceMap source_map_;
    memory::Arena scratch_arena_;
    memory::Arena ast_arena_;
    memory::Arena sym_arena_;
    memory::Arena type_arena_;
    memory::Arena hir_arena_;
    memory::Arena mir_arena_;
    memory::Arena zir_arena_;
    diagnostics::DiagnosticEngine diags_;

    ast::AstBuilder ast_builder_;
    import::ImportManager import_mgr_;
    import::SymbolTable syms_;
    memory::DynArray<import::SymId> resolved_syms_;
    types::TypeIntern types_;
    zir::hir::HirModule hir_module_;
    zir::mir::MirModule mir_module_;
    zir::ZirModule zir_module_;

    memory::FileId file_id_ = 0;
    lexer::TokenStream tokens_{};
    ast::ProgramNode program_{ast_arena_};
    parser::ScanResult scan_result_{ast_arena_};

    std::string output_buffer_;
    bool buffered_output_ = false;
    std::string content_override_; // non-empty = compile from buffer, not disk

#if defined(__GNUC__) || defined(__clang__)
    void writeOutput(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
    void writeOutput(const char *fmt, ...);
#endif

    const char *ansicolor(const char *code) const {
        return diags_.useColor() ? code : "";
    }

public:
    CompilationSession(const Options &opts, std::string file_path);

    bool run();
    bool runTo(Stage target);

    const diagnostics::DiagnosticEngine &diags() const {
        return diags_;
    }
    diagnostics::DiagnosticEngine &diags() {
        return diags_;
    }
    const std::string &filePath() const {
        return file_path_;
    }
    bool hasErrors() const {
        return diags_.hasErrors();
    }
    const ProjectConfig &projectConfig() const {
        return project_config_;
    }
    const memory::SourceMap &sourceMap() const {
        return source_map_;
    }
    memory::FileId fileId() const {
        return file_id_;
    }

    void setBuffered(bool b) {
        buffered_output_ = b;
        diags_.setSuppressEmit(b);
    }
    void setContent(std::string content) {
        content_override_ = std::move(content);
    }
    std::string flushOutput();
    void emitDiagnostics();

    // ── LSP feature accessors ─────────────────────────────────────────
    const lexer::TokenStream &tokens() const {
        return tokens_;
    }
    const parser::ScanResult &scanResult() const {
        return scan_result_;
    }
    const import::SymbolTable &symbolTable() const {
        return syms_;
    }
    import::SymbolTable &symbolTable() {
        return syms_;
    }
    const memory::DynArray<import::SymId> &resolvedSyms() const {
        return resolved_syms_;
    }
    const ast::AstBuilder &astBuilder() const {
        return ast_builder_;
    }
    ast::AstBuilder &astBuilder() {
        return ast_builder_;
    }
    const ast::ProgramNode &program() const {
        return program_;
    }
    const types::TypeIntern &types() const {
        return types_;
    }

    std::string fmtStage();

private:
    void setTarget(Stage s) {
        plan_.target = s;
    }
    bool lexStage();
    bool scanStage();
    bool importStage();
    bool resolveStage();
    bool semaStage();
    bool solveStage();
    bool nraStage();
    bool mirStage();
    bool zirStage();
};

} // namespace zith::cli
