#include <cli/pipeline/pipeline.hpp>
#include <cli/project_config/project_config.hpp>
#include <cli/cmd/commands.hpp>
#include <zith/zith.hpp>

#include <zith/ast.h>
//#include <lexer/debug.hpp>

#include <fstream>
#include <thread>
#include <vector>

namespace zith::cli::commands {

int cmd_check(const std::string &input_file, const std::string &mode_str, const bool verbose,
              const std::vector<std::string> &include_dirs) {
    std::string src = input_file;

    if (src.empty()) {
        zith::cli::project_config::ZithProject proj;
        if (!zith::cli::project_config::try_load_project(proj)) {
            zith::cli::pipeline::print_error("No input file and no ZithProject.toml found");
            return 1;
        }
        src = proj.entry;
    }

    if (verbose)
        zith::cli::pipeline::print_info("Checking '" + src + "' in " + mode_str + " mode...");

    ZithTokenStream stream{};
    const char *source = nullptr;
    size_t src_size = 0;
    ZithArena *arena =
        zith::cli::pipeline::tokenize_file(src, stream, &source, &src_size, verbose);
    if (!arena)
        return 1;

    //zith_debug_tokens(stream.data, stream.len);

    std::vector<const char *> import_roots;
    size_t import_root_count;
    zith::cli::project_config::build_import_roots(include_dirs, import_roots, import_root_count);

    ZithNode *ast = zith_parse_with_source(arena, source, src_size, src.c_str(), stream,
                                           import_roots.data(), import_root_count);

    if (verbose) {
        if (ast) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            printf("\n\n\n-Starting AST\n");
            zith_ast_print(ast, 0);
            printf("Ending AST\n\n\n");
        } else {
            zith::cli::pipeline::print_error("Parse failed — null AST\n\n");
            zith_arena_destroy(arena);
            return 1;
        }
    }

    if (!ast) {
        zith_arena_destroy(arena);
        return 1;
    }

    zith_arena_destroy(arena);
    zith::cli::pipeline::print_success("Check passed", src);
    return 0;
}

int cmd_compile(const std::string &input_file, const std::string &output_file,
                const std::string &mode_str, bool interpreted, bool verbose,
                const std::vector<std::string> &include_dirs) {
    (void)include_dirs;
    if (verbose) {
        const std::string kind = interpreted ? "bytecode" : "LLVM IR / native object";
        zith::cli::pipeline::print_info("Compiling '" + input_file + "' → " + kind + " (" + mode_str + ")");
    }

    ZithTokenStream stream{};
    const char *source = nullptr;
    size_t src_size = 0;
    ZithArena *arena =
        zith::cli::pipeline::tokenize_file(input_file, stream, &source, &src_size, verbose);
    if (!arena)
        return 1;

    std::vector<const char *> import_roots;
    size_t import_root_count;
    zith::cli::project_config::build_import_roots(include_dirs, import_roots, import_root_count);

    const ZithNode *ast = zith_parse_with_source(arena, source, src_size, input_file.c_str(), stream,
                                                 import_roots.data(), import_root_count);
    if (!ast) {
        zith_arena_destroy(arena);
        return 1;
    }

    const std::string out = !output_file.empty() ? output_file : (interpreted ? "a.zbc" : "a.o");
    if (interpreted) {
        std::ofstream ofs(out, std::ios::binary);
        if (!ofs) {
            zith::cli::pipeline::print_error("Failed to write bytecode file: " + out);
            zith_arena_destroy(arena);
            return 1;
        }
        ofs << source;
    }
    zith_arena_destroy(arena);
    zith::cli::pipeline::print_success(interpreted ? "Bytecode compile" : "Compile", out);
    return 0;
}

int cmd_build(const std::string &input_file, const std::string &output_file,
              const std::string &mode_str, bool verbose,
              const std::vector<std::string> &include_dirs) {
    std::string src = input_file;
    std::string out = output_file;
    std::string mode = mode_str;
    std::vector<std::string> extra_includes = include_dirs;

    if (src.empty()) {
        zith::cli::project_config::ZithProject proj;
        if (!zith::cli::project_config::try_load_project(proj)) {
            zith::cli::pipeline::print_error("No input file and no ZithProject.toml found");
            return 1;
        }
        src = proj.entry;
        if (out.empty())
            out = proj.output;
        if (mode == "debug")
            mode = proj.mode;

        extra_includes.insert(extra_includes.end(), proj.include_dirs.begin(),
                              proj.include_dirs.end());
    }

    if (verbose)
        zith::cli::pipeline::print_info("Building '" + src + "' → binary (" + mode + ")");

    if (const int rc = cmd_compile(src, "", mode, false, verbose, extra_includes); rc != 0)
        return rc;

    const std::string binary = out.empty() ? "a.out" : out;
    zith::cli::pipeline::print_success("Build", binary);
    return 0;
}

} // namespace zith::cli::commands
