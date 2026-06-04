#include "compilation-session.hpp"

namespace zith::cli {

    CompilationSession::CompilationSession(Options opts) :
        opts_(std::move(opts)), ast_arena_(), sym_arena_(), type_arena_(), hir_arena_(),
        mir_arena_() {}

    int CompilationSession::run() {
        if (!lexStage())
            return 1;
        if (!parseStage())
            return 1;
        if (!semaStage())
            return 1;
        if (!mirStage())
            return 1;
        if (!zirStage())
            return 1;
        return 0;
    }

    bool CompilationSession::lexStage() {
        return true;
    }
    bool CompilationSession::parseStage() {
        return true;
    }
    bool CompilationSession::semaStage() {
        return true;
    }
    bool CompilationSession::mirStage() {
        return true;
    }
    bool CompilationSession::zirStage() {
        return true;
    }

} // namespace zith::cli
