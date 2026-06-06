#include "compilation-session.hpp"

#include <cstdio>

namespace zith::cli {

CompilationSession::CompilationSession(Options opts) :
    opts_(std::move(opts)), ast_arena_(), sym_arena_(), type_arena_(), hir_arena_(),
    mir_arena_() {
    plan_.target = opts_.target_stage;
}

int CompilationSession::run() {
    if (!lexStage() || plan_.shouldStop())
        return diags_.hasErrors() ? 1 : 0;
    plan_.advance();
    if (!parseStage() || plan_.shouldStop())
        return diags_.hasErrors() ? 1 : 0;
    plan_.advance();
    if (!semaStage() || plan_.shouldStop())
        return diags_.hasErrors() ? 1 : 0;
    plan_.advance();
    if (!mirStage() || plan_.shouldStop())
        return diags_.hasErrors() ? 1 : 0;
    plan_.advance();
    if (!zirStage())
        return 1;
    return diags_.hasErrors() ? 1 : 0;
}

// TODO: wire up actual lexer pipeline (C++ lexer → TokenStream)
bool CompilationSession::lexStage() {
    // TODO: read file, run lexer, store tokens
    return true;
}

// TODO: wire up actual parser pipeline (TokenStream → AST)
bool CompilationSession::parseStage() {
    // TODO: run parser::Parser::parseProgram(), store AST
    return true;
}

// TODO: wire up sema pipeline (AST → type-checked AST)
bool CompilationSession::semaStage() {
    // TODO: run import resolver, type checker, HIR lowering
    return true;
}

// TODO: wire up MIR pipeline (HIR → MIR)
bool CompilationSession::mirStage() {
    // TODO: run HIR→MIR lowering, MIR verification
    return true;
}

// TODO: wire up ZIR pipeline (MIR → ZIR → interpretation / codegen)
bool CompilationSession::zirStage() {
    // TODO: run ZIR interpretation or LLVM codegen + emit
    return true;
}

} // namespace zith::cli
