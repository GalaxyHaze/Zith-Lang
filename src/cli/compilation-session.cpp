#include "compilation-session.hpp"
#include "ast/ast-printer.hpp"
#include "cli/terminal.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/resolver.hpp"
#include "lexer/lexer.hpp"
#include "memory/source-map.hpp"
#include "parser/parser.hpp"
#include "formatter/fmt-visitor.hpp"
#include "sema/heuristic-engine.hpp"
#include "sema/sema-pipeline.hpp"
#include "solve/solver.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <toml++/toml.hpp>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <unistd.h>

namespace zith::cli {

CompilationSession::CompilationSession(const Options &opts, std::string file_path)
    : opts_(opts), file_path_(std::move(file_path)), project_root_(), ast_arena_(), sym_arena_(),
      type_arena_(), hir_arena_(), mir_arena_(), scratch_arena_(), diags_(scratch_arena_),
      ast_builder_(ast_arena_), import_mgr_(sym_arena_, source_map_, diags_), syms_(sym_arena_),
      resolved_syms_(sym_arena_), types_(type_arena_), hir_module_(hir_arena_),
      mir_module_(mir_arena_), zir_module_(zir_arena_) {
    namespace fs = std::filesystem;
    if (fs::is_directory(file_path_))
        project_root_ = fs::weakly_canonical(fs::path(file_path_)).string();
    else
        project_root_ = fs::weakly_canonical(fs::path(file_path_).parent_path()).string();
    plan_.target = opts_.target_stage;
    diags_.setColor(term::useColor(opts_));
    diags_.setSourceMap(&source_map_);

    auto toml_path = fs::path(project_root_) / "ZithProject.toml";
    if (auto cfg = ProjectConfig::load(toml_path.string()))
        project_config_ = std::move(*cfg);
}

bool CompilationSession::run() {
    return runTo(plan_.target);
}

bool CompilationSession::runTo(Stage target) {
    auto t_start = std::chrono::steady_clock::now();
    plan_.target = target;

    if (opts_.verbose)
        writeOutput("%s[zithc] [starting]%s %s\n", ansicolor("\033[36m"), ansicolor("\033[0m"),
                    file_path_.c_str());

    // Stage 1: Lex
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!lexStage())
        return false;
    plan_.advance();

    // Stage 2: Scan (register symbols, create UnbodyNodes)
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!scanStage())
        return false;
    plan_.advance();

    // Stage 3: Import resolution
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!importStage())
        return false;
    plan_.advance();

    // Stage 4: Name resolution
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!resolveStage())
        return false;
    plan_.advance();

    // Stage 5: Type checking + body expansion + HIR lowering
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!semaStage())
        return false;
    plan_.advance();

    // Stage 6: Solver (generic instantiation, monomorphization)
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!solveStage())
        return false;
    plan_.advance();

    // Stage 7: MIR lowering
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!mirStage())
        return false;
    plan_.advance();

    // Stage 8: ZIR interpretation
    if (plan_.shouldStop())
        return !diags_.hasErrors();
    if (!zirStage())
        return false;

    bool ok = !diags_.hasErrors();
    if (opts_.verbose) {
        auto dt =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_start)
                .count();
        writeOutput("%s[zithc] [done]%s %s %s%s%s (%.1fms)\n", ansicolor("\033[36m"),
                    ansicolor("\033[0m"), file_path_.c_str(),
                    ansicolor(ok ? "\033[32m" : "\033[31m"), ok ? "\xe2\x9c\x93" : "\xe2\x9c\x97",
                    ansicolor("\033[0m"), dt);
    }
    return ok;
}

bool CompilationSession::lexStage() {
    auto t0      = std::chrono::steady_clock::now();
    namespace fs = std::filesystem;

    if (fs::is_directory(file_path_)) {
        if (project_config_.entry.empty()) {
            writeOutput("%s[error]%s no entry file in ZithProject.toml\n", ansicolor("\033[31m"),
                        ansicolor("\033[0m"));
            return false;
        }
        file_path_ = (fs::path(project_root_) / project_config_.entry).string();
    }

    if (opts_.verbose) {
        std::error_code ec;
        auto fsize = fs::file_size(file_path_, ec);
        if (ec)
            writeOutput("[file] %s\n", file_path_.c_str());
        else
            writeOutput("[file] %s (%.1f KiB)\n", file_path_.c_str(), fsize / 1024.0);
    }

    auto file_result = !content_override_.empty()
        ? source_map_.addFile(file_path_, content_override_)
        : source_map_.loadFile(file_path_);
    if (!file_result) {
        writeOutput("%s[error]%s failed to load file '%s'\n", ansicolor("\033[31m"),
                    ansicolor("\033[0m"), file_path_.c_str());
        return false;
    }
    file_id_ = file_result.value();

    auto token_result = lexer::tokenize(source_map_, scratch_arena_, file_id_, diags_);
    if (!token_result) {
        diags_.emit();
        return false;
    }
    tokens_ = token_result.value();

    if (opts_.print_tokens || opts_.emit_tokens)
        lexer::printTokens(tokens_);

    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [lex] %6u tokens  (%5.1fms)\n", tokens_.len, dt);
    }

    return true;
}

bool CompilationSession::importStage() {
    auto t0      = std::chrono::steady_clock::now();
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
            exe_dir      = fs::path(exe_buf).parent_path();
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
            exe_dir          = fs::path(exe_buf).parent_path();
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

    import_mgr_.setVisibleRoots(std::move(visible_roots));

    auto source_dir = fs::path(file_path_).parent_path().string();

    for (auto decl_id : program_.decls) {
        auto &decl = ast_builder_.getDecl(decl_id);
        if (auto *import = std::get_if<ast::ImportNode>(&decl)) {
            auto res = import_mgr_.resolve(import->path, import->is_from, import->is_export,
                                           import->alias, import->import_depth, source_dir);
            if (!res) {
                diags_.report(diagnostics::Severity::Error, diagnostics::err::ImportError,
                              std::string(res.error().msg), import->span);
                continue;
            }
        }
    }

    import_mgr_.mergeInto(syms_);

    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [import] %zu symbols  (%5.1fms)\n", syms_.symbolCount(), dt);
    }

    return true;
}

bool CompilationSession::resolveStage() {
    auto t0 = std::chrono::steady_clock::now();

    import::Resolver resolver(syms_, import_mgr_, ast_builder_, diags_);
    resolver.resolveProgram(program_);
    resolved_syms_ = resolver.takeResolvedTable();

    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [resolve] %5.1fms\n", dt);
    }

    return !diags_.hasErrors();
}

bool CompilationSession::scanStage() {
    auto t0 = std::chrono::steady_clock::now();
    parser::Parser parser(&tokens_, &ast_builder_, &diags_);
    scan_result_ = parser::scan(parser, syms_);
    program_     = std::move(parser.program);
    if (diags_.hasErrors()) {
        diags_.emit();
        return false;
    }
    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [scan] %6zu top-level decls  (%5.1fms)\n", program_.decls.size(), dt);
    }
    return true;
}

bool CompilationSession::semaStage() {
    auto t0 = std::chrono::steady_clock::now();

    if (diags_.hasErrors()) {
        diags_.emit();
        return false;
    }

    // Expand unbody nodes into real AST before type-checking
    {
        parser::Parser parser(&tokens_, &ast_builder_, &diags_);
        parser.program = std::move(program_);
        parser.expandBodies(scan_result_);
        program_ = std::move(parser.program);
    }

    if (opts_.emit_ast) {
        std::printf("--- AST ---\n");
        ast::printAST(program_, ast_builder_);
        std::printf("--- Symbols ---\n");
        syms_.dump();
        std::printf("---\n");
    }

    sema::SemaPipeline pipeline(syms_, types_, diags_, ast_builder_, hir_arena_, &resolved_syms_);
    if (!pipeline.run(program_)) {
        diags_.emit();
        return false;
    }

    hir_module_ = pipeline.takeHir();

    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [sema] %zu fns lowered  (%5.1fms)\n", hir_module_.getFnCount(), dt);
    }

    return !diags_.hasErrors();
}

bool CompilationSession::solveStage() {
    auto t0 = std::chrono::steady_clock::now();
    solve::Solver solver(types_, ast_builder_, program_, syms_, diags_, hir_arena_);
    if (!solver.solve(hir_module_)) {
        diags_.emit();
        return false;
    }
    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [solve] \xe2\x80\x94 (stub)  (%5.1fms)\n", dt);
    }
    return true;
}

bool CompilationSession::mirStage() {
    auto t0 = std::chrono::steady_clock::now();
    // TODO: wire up HIR → MIR lowering, MIR verification
    // Thread-safety: each session owns its own MirModule — safe to parallelize.
    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [mir] %zu fns  (%5.1fms)\n", mir_module_.fnCount(), dt);
    }
    return true;
}

bool CompilationSession::zirStage() {
    auto t0 = std::chrono::steady_clock::now();
    // TODO: wire up ZIR interpretation or LLVM codegen + emit
    // Serialization point: after this stage, results from multiple files
    // may need to be merged (linking, combining interpreted modules).
    // This is the only stage that may require inter-file synchronization.
    if (opts_.verbose) {
        auto dt = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                      .count();
        writeOutput("  [zir] \xe2\x80\x94 (stub)  (%5.1fms)\n", dt);
    }
    return true;
}

std::string CompilationSession::fmtStage() {
    // Run lex + scan
    if (!lexStage())
        return {};
    if (!scanStage())
        return {};

    // Expand unbody nodes into real AST
    {
        parser::Parser parser(&tokens_, &ast_builder_, &diags_);
        parser.program = std::move(program_);
        parser.expandBodies(scan_result_);
        program_ = std::move(parser.program);
    }

    // Run formatter
    formatter::FmtVisitor visitor(ast_builder_, program_);
    visitor.format();
    return visitor.result();
}

void CompilationSession::writeOutput(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (buffered_output_) {
        va_list args_copy;
        va_copy(args_copy, args);
        int len = std::vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);
        if (len > 0) {
            auto old = output_buffer_.size();
            output_buffer_.resize(old + len);
            std::vsnprintf(output_buffer_.data() + old, len + 1, fmt, args);
        }
    } else {
        std::vfprintf(stderr, fmt, args);
    }
    va_end(args);
}

std::string CompilationSession::flushOutput() {
    auto result = std::move(output_buffer_);
    output_buffer_.clear();
    return result;
}

void CompilationSession::emitDiagnostics() {
    sema::HeuristicEngine heuristic;
    auto &diags = diags_.diagnostics();
    for (size_t i = 0; i < diags.size(); i++) {
        heuristic.generate(diags[i], syms_, diags[i].suggestions);
    }
    diags_.setSuppressEmit(false);
    diags_.emit();
}

} // namespace zith::cli
