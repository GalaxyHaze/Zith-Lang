#include "compilation-session.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "parser/source-map.hpp"
#include "import/resolver.hpp"

#include <cstdio>

namespace zith::cli {

CompilationSession::CompilationSession(const Options &opts, std::string file_path) :
    opts_(opts),
    file_path_(std::move(file_path)),
    ast_arena_(),
    sym_arena_(),
    type_arena_(),
    hir_arena_(),
    mir_arena_(),
    ast_builder_(ast_arena_),
    syms_(sym_arena_),
    types_(type_arena_),
    hir_module_(hir_arena_),
    mir_module_(mir_arena_)
{
    plan_.target = opts_.target_stage;
}

bool CompilationSession::run() {
    return runTo(plan_.target);
}

bool CompilationSession::runTo(Stage target) {
    plan_.target = target;

    if (!lexStage()) return false;
    if (plan_.shouldStop()) return !diags_.hasErrors();
    plan_.advance();

    if (!parseStage()) return false;
    if (plan_.shouldStop()) return !diags_.hasErrors();
    plan_.advance();

    if (!semaStage()) return false;
    if (plan_.shouldStop()) return !diags_.hasErrors();
    plan_.advance();

    if (!mirStage()) return false;
    if (plan_.shouldStop()) return !diags_.hasErrors();
    plan_.advance();

    if (!zirStage()) return false;

    return !diags_.hasErrors();
}

bool CompilationSession::lexStage() {
    auto file_result = parser::SourceMap::load_file(file_path_);
    if (!file_result) {
        std::fprintf(stderr, "[error] failed to load file '%s'\n", file_path_.c_str());
        return false;
    }
    file_id_ = file_result.value();

    auto token_result = lexer::tokenize(file_id_, diags_);
    if (!token_result) {
        diags_.emit();
        return false;
    }
    tokens_ = token_result.value();

    if (opts_.print_tokens || opts_.emit_tokens)
        lexer::printTokens(tokens_);

    return true;
}

bool CompilationSession::parseStage() {
    parser::Parser parser(tokens_, ast_builder_, diags_);
    auto result = parser.parseProgram();
    program_ = result.value;

    // If diagnostics were emitted during parsing, report them
    if (diags_.hasErrors())
        diags_.emit();

    // Even if the parser is currently stubbed (ok=false),
    // consider success as long as no real errors were reported
    return !diags_.hasErrors();
}

bool CompilationSession::semaStage() {
    // TODO: wire up import resolution, type checker, HIR lowering
    // Thread-safety: each session owns its own SymbolTable, TypeIntern,
    // and HirModule — safe to parallelize across files.
    import::Resolver resolver(syms_, ast_builder_, diags_);
    resolver.resolveProgram(program_);

    if (diags_.hasErrors()) {
        diags_.emit();
        return false;
    }
    return true;
}

bool CompilationSession::mirStage() {
    // TODO: wire up HIR → MIR lowering, MIR verification
    // Thread-safety: each session owns its own MirModule — safe to parallelize.
    return true;
}

bool CompilationSession::zirStage() {
    // TODO: wire up ZIR interpretation or LLVM codegen + emit
    // Serialization point: after this stage, results from multiple files
    // may need to be merged (linking, combining interpreted modules).
    // This is the only stage that may require inter-file synchronization.
    return true;
}

} // namespace zith::cli
