#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "cli/options.hpp"
#include "cli/pipeline-plan.hpp"
#include "ast/ast-builder.hpp"
#include "lexer/token.hpp"
#include "memory/source-map.hpp"
#include "parser/scan-result.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/mir/mir-module.hpp"
#include "import/symbol-table.hpp"
#include "import/import-manager.hpp"
#include "types/type-intern.hpp"
#include "cli/project-config.hpp"

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
//   - SessionArena is already thread_local
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

    diagnostics::DiagnosticEngine diags_;
    memory::Arena ast_arena_;
    memory::Arena sym_arena_;
    memory::Arena type_arena_;
    memory::Arena hir_arena_;
    memory::Arena mir_arena_;

    ast::AstBuilder ast_builder_;
    import::SymbolTable syms_;
    types::TypeIntern types_;
    zir::hir::HirModule hir_module_;
    zir::mir::MirModule mir_module_;

    memory::FileId file_id_ = 0;
    lexer::TokenStream tokens_{};
    ast::ProgramNode program_{memory::DynArray<ast::DeclId>{ast_arena_}};
    parser::ScanResult scan_result_{ast_arena_};

public:
    CompilationSession(const Options &opts, std::string file_path);

    bool run();
    bool runTo(Stage target);

    const diagnostics::DiagnosticEngine &diags() const { return diags_; }
    const std::string &filePath() const { return file_path_; }
    bool hasErrors() const { return diags_.hasErrors(); }
    const ProjectConfig &projectConfig() const { return project_config_; }

private:
    void setTarget(Stage s) { plan_.target = s; }
    bool lexStage();
    void scanStage();
    void expandBodiesStage();
    void solveStage();
    bool parseStage();
    bool importStage();
    bool semaStage();
    bool mirStage();
    bool zirStage();
};

} // namespace zith::cli
