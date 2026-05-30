#pragma once

#include "diagnostics/engine/diagnostic-engine.hpp"
#include "driver/options.hpp"
#include "driver/pipeline-plan.hpp"
#include "frontend/ast/ast-builder.hpp"
#include "frontend/lexer/token.hpp"
#include "frontend/source/source-map.hpp"
#include "middleend/hir/hir-module.hpp"
#include "middleend/mir/mir-module.hpp"
#include "middleend/symbols/symbol-table.hpp"
#include "middleend/types/type-intern.hpp"

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
        bool backendStage();
    };

} // namespace zith::driver
