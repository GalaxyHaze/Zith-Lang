#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "cli/options.hpp"
#include "cli/pipeline-plan.hpp"
#include "ast/ast-builder.hpp"
#include "lexer/token.hpp"
#include "parser/source-map.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/mir/mir-module.hpp"
#include "import/symbol-table.hpp"
#include "types/type-intern.hpp"

namespace zith::driver {

    class CompilationSession {
        Options opts_;
        PipelinePlan plan_;
        diagnostics::engine::DiagnosticEngine diags_;

        infra::alloc::Arena ast_arena_;
        infra::alloc::Arena sym_arena_;
        infra::alloc::Arena type_arena_;
        infra::alloc::Arena hir_arena_;
        infra::alloc::Arena mir_arena_;

    public:
        explicit CompilationSession(Options opts);

        int run();

    private:
        bool lexStage();
        bool parseStage();
        bool semaStage();
        bool mirStage();
        bool zirStage();
    };

} // namespace zith::driver
