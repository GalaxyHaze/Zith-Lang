#include "cli/pipeline/pipeline.hpp"
#include "cli/project_config/project_config.hpp"
#include "cli/runtime_interpreted/runtime_interpreted.hpp"
#include "cli/cmd/commands.hpp"
#include <zith/zith.hpp>

#include <fstream>
#include <vector>

namespace zith::cli::commands {

int cmd_execute(const std::string &target, const bool interpreted, const bool verbose,
                const std::vector<std::string> &include_dirs) {
    std::string bin = target;

    if (bin.empty()) {
        zith::cli::project_config::ZithProject proj;
        if (!zith::cli::project_config::try_load_project(proj)) {
            zith::cli::pipeline::print_error("No target specified and no ZithProject.toml found");
            return 1;
        }
        bin = interpreted ? (proj.output + ".nbc") : proj.output;
    }

    if (!zith_file_exists(bin.c_str())) {
        const std::string hint = interpreted ? "compile --interpreted" : "build";
        zith::cli::pipeline::print_error("Target not found: '" + bin + "' -- did you run 'zith " + hint + "' first?");
        return 1;
    }

    if (verbose)
        zith::cli::pipeline::print_info("Executing '" + bin + "'" + (interpreted ? " (interpreted)" : ""));

    if (interpreted) {
        std::ifstream ifs(bin, std::ios::binary);
        if (!ifs) {
            zith::cli::pipeline::print_error("Failed to read bytecode file: " + bin);
            return 1;
        }
        std::string source((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ZithArena *arena = zith_arena_create(64 * 1024);
        if (!arena)
            return 1;
        ZithTokenStream stream = zith_tokenize(arena, source.c_str(), source.size());
        if (!stream.data) {
            zith_arena_destroy(arena);
            return 1;
        }
        std::vector<std::string> import_roots;
        zith::cli::project_config::build_import_roots(bin, include_dirs, import_roots);
        std::vector<const char *> import_root_ptrs;
        for (const auto &s : import_roots) import_root_ptrs.push_back(s.c_str());

        ZithNode *ast = zith_parse_with_source(arena, source.c_str(), source.size(), bin.c_str(),
                                               stream, import_root_ptrs.data(), import_root_ptrs.size());
        if (!ast) {
            zith_arena_destroy(arena);
            return 1;
        }
        const int rc = zith::cli::runtime_interpreted::run_interpreted_ast(ast);
        zith_arena_destroy(arena);
        return rc;
    }
    zith::cli::pipeline::print_not_implemented("execute");
    return 1;
}

int cmd_run(const std::string &input_file, const std::string &output_file,
            const std::string &mode_str, bool interpreted, bool verbose,
            const std::vector<std::string> &include_dirs) {
    if (interpreted) {
        const std::string bc = output_file.empty() ? "a.nbc" : output_file;
        if (const int rc = cmd_compile(input_file, bc, mode_str, true, verbose, include_dirs);
            rc != 0)
            return rc;
        return cmd_execute(bc, true, verbose, include_dirs);
    }

    if (const int rc = cmd_build(input_file, output_file, mode_str, verbose, include_dirs); rc != 0)
        return rc;

    const std::string binary = output_file.empty() ? "a.out" : output_file;
    return cmd_execute(binary, false, verbose, include_dirs);
}

} // namespace zith::cli::commands
