#include "project_config.hpp"

#include <zith/zith.hpp>

namespace zith::cli::project_config {

bool try_load_project(ZithProject &proj) {
    if (!zith_file_exists("ZithProject.toml")) return false;
    proj = ZithProject{};
    return true;
}

void build_import_roots(const std::vector<std::string> &extra_dirs,
                        std::vector<const char *> &roots_out,
                        size_t &count_out) {
    static const char *default_roots[] = {"std", "utils", "c"};
    constexpr size_t default_count = 3;

    const size_t total = default_count + extra_dirs.size();
    roots_out.resize(total);
    for (size_t i = 0; i < default_count; ++i) roots_out[i] = default_roots[i];
    for (size_t i = 0; i < extra_dirs.size(); ++i) roots_out[default_count + i] = extra_dirs[i].c_str();
    count_out = total;
}

} // namespace zith::cli::project_config
