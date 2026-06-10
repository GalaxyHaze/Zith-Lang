#include "compilation-session.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "ast/ast-printer.hpp"
#include "memory/source-map.hpp"
#include "import/resolver.hpp"
#include "diagnostics/error-codes.hpp"

#include <cstdio>
#include <filesystem>
#include <vector>
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
    project_root_(),
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
    namespace fs = std::filesystem;
    if (fs::is_directory(file_path_))
        project_root_ = fs::weakly_canonical(fs::path(file_path_)).string();
    else
        project_root_ = fs::weakly_canonical(fs::path(file_path_).parent_path()).string();
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

    importStage();
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

bool CompilationSession::importStage() {
    namespace fs = std::filesystem;

    // ── Compute visible roots ──────────────────────────────────────
    std::vector<std::string> visible_roots;

    // 1. Stdlib path (relative to compiler executable)
    {
        char exe_buf[4096];
        ssize_t exe_len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (exe_len != -1) {
            exe_buf[exe_len] = '\0';
            fs::path exe_dir = fs::path(exe_buf).parent_path();
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

    // 2. -I include dirs from CLI
    for (auto &d : opts_.include_dirs)
        visible_roots.push_back(fs::weakly_canonical(fs::path(d)).string());

    // 3. Project root
    if (!project_root_.empty())
        visible_roots.push_back(project_root_);

    // 4. ZithProject.toml [paths] entries
    if (!project_root_.empty()) {
        auto toml_path = fs::path(project_root_) / "ZithProject.toml";
        if (fs::exists(toml_path)) {
            try {
                auto tbl = toml::parse_file(toml_path.string());
                if (auto *paths = tbl["paths"].as_table()) {
                    auto add_path = [&](std::string_view key) {
                        if (auto val = paths->get(key)->value<std::string>()) {
                            auto p = fs::weakly_canonical(fs::path(project_root_) / *val);
                            visible_roots.push_back(p.string());
                        }
                    };
                    if (paths->contains("src_dir"))  add_path("src_dir");
                    if (paths->contains("mod_dir"))  add_path("mod_dir");
                    if (paths->contains("test_dir")) add_path("test_dir");
                }
            } catch (...) {}
        }
    }

    import::ImportManager import_mgr{sym_arena_, diags_, std::move(visible_roots)};

    auto source_dir = fs::path(file_path_).parent_path().string();

    for (auto decl_id : program_.decls) {
        auto &decl = ast_builder_.getDecl(decl_id);
        if (auto *import = std::get_if<ast::ImportNode>(&decl)) {
            auto res = import_mgr.resolve(import->path, import->is_from,
                                          import->is_export, import->alias,
                                          import->import_depth, source_dir);
            if (!res) {
                diags_.report(diagnostics::Severity::Error,
                              diagnostics::err::ImportError,
                              std::string(res.error().msg), {});
                continue;
            }
        }
    }

    import_mgr.mergeInto(syms_);
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
