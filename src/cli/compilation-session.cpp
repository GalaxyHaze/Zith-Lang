#include "compilation-session.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "ast/ast-printer.hpp"
#include "memory/source-map.hpp"
#include "import/resolver.hpp"

#include <cstdio>
#include <filesystem>
#include <toml++/toml.hpp>
#include <unistd.h>

namespace zith::cli {

static bool shouldUseColor(const std::string &setting) {
    if (setting == "on")
        return true;
    if (setting == "off")
        return false;
    // auto: enable if stderr is a TTY
    return isatty(fileno(stderr)) != 0;
}

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
    diags_.setColor(shouldUseColor(opts_.color));
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
    namespace fs = std::filesystem;

    // If the path is a directory, resolve it to the project's entry file
    if (fs::is_directory(file_path_)) {
        auto toml_path = fs::path(file_path_) / "ZithProject.toml";
        if (!fs::exists(toml_path)) {
            std::fprintf(stderr, "[error] no ZithProject.toml found in '%s'\n",
                         file_path_.c_str());
            return false;
        }
        try {
            auto tbl = toml::parse_file(toml_path.string());
            if (auto *build = tbl["build"].as_table()) {
                if (auto entry = build->get("entry")) {
                    if (auto val = entry->value<std::string>()) {
                        file_path_ = (fs::path(file_path_) / *val).string();
                    }
                }
            }
        } catch (const toml::parse_error &e) {
            std::fprintf(stderr, "[error] failed to parse '%s': %.*s\n",
                         toml_path.string().c_str(),
                         (int)e.description().size(), e.description().data());
            return false;
        }
    }

    auto file_result = memory::SourceMap::load_file(file_path_);
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
    scanStage();
    if (diags_.hasErrors()) { diags_.emit(); return false; }

    expandBodiesStage();
    if (diags_.hasErrors()) { diags_.emit(); return false; }

    solveStage();
    if (diags_.hasErrors()) { diags_.emit(); return false; }

    if (opts_.emit_ast) {
        std::printf("--- AST ---\n");
        ast::printAST(program_, ast_builder_);
        std::printf("--- Symbols ---\n");
        syms_.dump();
        std::printf("---\n");
    }

    return true;
}

void CompilationSession::scanStage() {
    parser::Parser parser(&tokens_, &ast_builder_, &diags_);
    scan_result_ = parser::scan(parser, syms_);
    program_ = std::move(parser.program);
}

void CompilationSession::expandBodiesStage() {
    parser::Parser parser(&tokens_, &ast_builder_, &diags_);
    parser.program = std::move(program_);
    parser.expandBodies(scan_result_);
    program_ = std::move(parser.program);
}

void CompilationSession::solveStage() {
    // TODO: generics, macros, comptime
}

bool CompilationSession::semaStage() {
    // TODO: wire up type checker, HIR lowering
    // Thread-safety: each session owns its own SymbolTable, TypeIntern,
    // and HirModule — safe to parallelize across files.
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
