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
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace zith::cli {

static bool shouldUseColor(const std::string &setting) {
    if (setting == "on")
        return true;
    if (setting == "off")
        return false;
    // auto: enable if stderr is a TTY
#ifdef _WIN32
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
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
    diags_.setSourceMap(&source_map_);

    auto toml_path = fs::path(project_root_) / "ZithProject.toml";
    if (auto cfg = ProjectConfig::load(toml_path.string()))
        project_config_ = std::move(*cfg);
}

bool CompilationSession::run() {
    return runTo(plan_.target);
}

bool CompilationSession::runTo(Stage target) {
    plan_.target = target;

    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!lexStage()) return false;
    plan_.advance();

    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!parseStage()) return false;
    plan_.advance();

    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!semaStage()) return false;
    plan_.advance();

    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!mirStage()) return false;
    plan_.advance();

    if (plan_.shouldStop()) return !diags_.hasErrors();
    if (!zirStage()) return false;

    return !diags_.hasErrors();
}

bool CompilationSession::lexStage() {
    namespace fs = std::filesystem;

    if (fs::is_directory(file_path_)) {
        if (project_config_.entry.empty()) {
            std::fprintf(stderr, "[error] no entry file in ZithProject.toml\n");
            return false;
        }
        file_path_ = (fs::path(project_root_) / project_config_.entry).string();
    }

    auto file_result = source_map_.loadFile(file_path_);
    if (!file_result) {
        std::fprintf(stderr, "[error] failed to load file '%s'\n", file_path_.c_str());
        return false;
    }
    file_id_ = file_result.value();

    auto token_result = lexer::tokenize(source_map_, file_id_, diags_);
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
        fs::path exe_dir;
        char exe_buf[4096];
#ifdef _WIN32
        DWORD len = GetModuleFileNameA(NULL, exe_buf, sizeof(exe_buf));
        if (len != 0 && len != sizeof(exe_buf)) {
            exe_buf[len] = '\0';
            exe_dir = fs::path(exe_buf).parent_path();
        }
#elif defined(__APPLE__)
        uint32_t size = sizeof(exe_buf);
        if (_NSGetExecutablePath(exe_buf, &size) == 0) {
            exe_dir = fs::path(exe_buf).parent_path();
        }
#else
        ssize_t exe_len = readlink("/proc/self/exe", exe_buf, sizeof(exe_buf) - 1);
        if (exe_len != -1) {
            exe_buf[exe_len] = '\0';
            exe_dir = fs::path(exe_buf).parent_path();
        }
#endif
        if (!exe_dir.empty()) {
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

    // 4. src_dirs from ZithProject.toml [paths] (list)
    for (const auto &sd : project_config_.src_dirs) {
        auto p = fs::weakly_canonical(fs::path(project_root_) / sd);
        visible_roots.push_back(p.string());
    }
    // 5. mod_dir, test_dir from ZithProject.toml [paths]
    if (!project_config_.mod_dir.empty()) {
        auto p = fs::weakly_canonical(fs::path(project_root_) / project_config_.mod_dir);
        visible_roots.push_back(p.string());
    }
    if (!project_config_.test_dir.empty()) {
        auto p = fs::weakly_canonical(fs::path(project_root_) / project_config_.test_dir);
        visible_roots.push_back(p.string());
    }

    import::ImportManager import_mgr{sym_arena_, source_map_, diags_, std::move(visible_roots)};

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
    parser::Parser parser{&tokens_, &ast_builder_, &diags_};
    scan_result_ = parser::scan(parser, syms_);
    program_ = std::move(parser.program);
}

void CompilationSession::expandBodiesStage() {
    parser::Parser parser{&tokens_, &ast_builder_, &diags_};
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
