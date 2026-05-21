#include "project_config.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <toml++/toml.hpp>
#include <zith/zith.hpp>

namespace zith::cli::project_config {

namespace {
constexpr const char *kRequiredKeys[] = {"name", "version", "entry"};
constexpr const char *kKnownKeys[]    = {
    "name",      "version",     "description", "authors",       "license",      "homepage",
    "entry",     "output",      "mode",        "target_triple", "edition",      "src_dir",
    "bin_dir",   "lib_dir",     "docs_dir",    "test_dir",      "cache_dir",    "include_dirs",
    "lib_paths", "link_libs",   "link_flags",  "features",      "dependencies", "emit_ir",
    "emit_asm",  "strip_debug", "lto",         "opt_level",     "debug_level"};

bool is_known_key(const std::string &key) {
    for (const auto *known : kKnownKeys) {
        if (key == known)
            return true;
    }
    return false;
}

void set_string_field(const toml::table &tbl, const char *key, std::string &field,
                      std::vector<std::string> &errors) {
    if (auto node = tbl.get(key)) {
        if (auto value = node->value<std::string>())
            field = *value;
        else
            errors.emplace_back(std::string("Field '") + key + "' must be a string");
    }
}

} // namespace

bool try_load_project_from_path(ZithProject &proj, const std::string &project_file,
                                std::vector<std::string> *warnings,
                                std::vector<std::string> *errors) {
    std::vector<std::string> local_warnings;
    std::vector<std::string> local_errors;

    proj = ZithProject{};
    toml::table tbl;
    try {
        tbl = toml::parse_file(project_file);
    } catch (const toml::parse_error &e) {
        local_errors.emplace_back("Failed parsing TOML: " + std::string(e.description()));
        if (warnings)
            *warnings = std::move(local_warnings);
        if (errors)
            *errors = std::move(local_errors);
        return false;
    }

    for (const auto &[k, _] : tbl) {
        if (!is_known_key(std::string(k.str()))) {
            local_warnings.emplace_back("Unknown key in ZithProject.toml: '" +
                                        std::string(k.str()) + "'");
        }
    }

    for (const auto *req : kRequiredKeys) {
        if (!tbl.contains(req))
            local_errors.emplace_back(std::string("Missing required field '") + req + "'");
    }

    set_string_field(tbl, "name", proj.name, local_errors);
    set_string_field(tbl, "version", proj.version, local_errors);
    set_string_field(tbl, "entry", proj.entry, local_errors);

    set_string_field(tbl, "description", proj.description, local_errors);
    set_string_field(tbl, "authors", proj.authors, local_errors);
    set_string_field(tbl, "license", proj.license, local_errors);
    set_string_field(tbl, "homepage", proj.homepage, local_errors);
    set_string_field(tbl, "output", proj.output, local_errors);
    set_string_field(tbl, "mode", proj.mode, local_errors);
    set_string_field(tbl, "target_triple", proj.target_triple, local_errors);
    set_string_field(tbl, "edition", proj.edition, local_errors);
    set_string_field(tbl, "src_dir", proj.src_dir, local_errors);
    set_string_field(tbl, "bin_dir", proj.bin_dir, local_errors);
    set_string_field(tbl, "lib_dir", proj.lib_dir, local_errors);
    set_string_field(tbl, "docs_dir", proj.docs_dir, local_errors);
    set_string_field(tbl, "test_dir", proj.test_dir, local_errors);
    set_string_field(tbl, "cache_dir", proj.cache_dir, local_errors);

    if (auto opt = tbl["opt_level"].value<int64_t>()) {
        if (*opt < 0 || *opt > 3)
            local_errors.emplace_back("Field 'opt_level' must be in range 0..3");
        else
            proj.opt_level = static_cast<int>(*opt);
    } else if (tbl.contains("opt_level")) {
        local_errors.emplace_back("Field 'opt_level' must be an integer");
    }

    if (const auto dbg = tbl["debug_level"].value<int64_t>()) {
        if (*dbg < 0 || *dbg > 3)
            local_errors.emplace_back("Field 'debug_level' must be in range 0..3");
        else
            proj.debug_level = static_cast<int>(*dbg);
    } else if (tbl.contains("debug_level")) {
        local_errors.emplace_back("Field 'debug_level' must be an integer");
    }

    const std::filesystem::path base_dir =
        std::filesystem::absolute(std::filesystem::path(project_file)).parent_path();
    proj.entry = (base_dir / proj.entry).lexically_normal().string();

    if (warnings)
        *warnings = std::move(local_warnings);
    if (errors)
        *errors = std::move(local_errors);
    return errors ? errors->empty() : local_errors.empty();
}

bool try_load_project(ZithProject &proj) {
    constexpr auto path = "ZithProject.toml";
    if (!std::filesystem::exists(path))
        return false;

    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    const bool ok = try_load_project_from_path(proj, path, &warnings, &errors);
    for (const auto &w : warnings) {
        std::cerr << "[warning] " << w << "\n";
    }
    for (const auto &e : errors) {
        std::cerr << "[error] " << e << "\n";
    }
    return ok;
}

void build_import_roots(const std::string &source_file,
                        const std::vector<std::string> &extra_dirs,
                        std::vector<std::string> &roots_out) {
    roots_out.clear();

    auto add_subdirs = [&](const std::filesystem::path &base) {
        if (!std::filesystem::exists(base) || !std::filesystem::is_directory(base))
            return;
        for (const auto &entry : std::filesystem::directory_iterator(base)) {
            if (entry.is_directory()) {
                roots_out.push_back(entry.path().string());
            }
        }
    };

    auto find_lib_in_ancestors = [&]() {
        std::filesystem::path dir = std::filesystem::absolute(source_file).parent_path();
        for (int i = 0; i < 10 && !dir.empty(); ++i) {
            auto lib_path = dir / "lib";
            if (std::filesystem::exists(lib_path) && std::filesystem::is_directory(lib_path)) {
                add_subdirs(lib_path);
                return;
            }
            dir = dir.parent_path();
        }
    };

    find_lib_in_ancestors();
    add_subdirs(std::filesystem::absolute("lib"));

#ifdef ZITH_INSTALL_LIB_DIR
    add_subdirs(std::filesystem::path(ZITH_INSTALL_LIB_DIR));
#endif

    std::ranges::sort(roots_out);
    roots_out.erase(std::ranges::unique(roots_out).begin(), roots_out.end());

    for (const auto &dir : extra_dirs) {
        roots_out.push_back(dir);
    }
}

} // namespace zith::cli::project_config