#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

namespace zith::cli {

struct ProjectConfig {
    // [build]
    std::string entry;
    std::string output = "a.out";
    std::string mode   = "debug";
    int opt_level      = 0;

    // [paths]
    std::vector<std::string> src_dirs;
    std::string bin_dir  = "target";
    std::string mod_dir  = ".zmodules";
    std::string docs_dir = "docs";
    std::string test_dir = "test";
    std::string asset_dir;

    // [project]
    std::string name;
    std::string version;
    std::string description;
    std::string authors;
    std::string license;
    std::string homepage;

    static std::optional<ProjectConfig> load(const std::string &toml_path) {
        namespace fs = std::filesystem;
        if (!fs::exists(toml_path))
            return std::nullopt;

        auto read_str = [](const toml::table *tbl,
                           std::string_view key) -> std::optional<std::string> {
            if (auto *val = tbl->get(key))
                return val->value<std::string>();
            return std::nullopt;
        };

        auto process = [&](const toml::table &tbl) -> std::optional<ProjectConfig> {
            ProjectConfig cfg;

            if (auto *proj = tbl["project"].as_table()) {
                if (auto v = read_str(proj, "name"))
                    cfg.name = *v;
                if (auto v = read_str(proj, "version"))
                    cfg.version = *v;
                if (auto v = read_str(proj, "description"))
                    cfg.description = *v;
                if (auto v = read_str(proj, "authors"))
                    cfg.authors = *v;
                if (auto v = read_str(proj, "license"))
                    cfg.license = *v;
                if (auto v = read_str(proj, "homepage"))
                    cfg.homepage = *v;
            }

            if (auto *build = tbl["build"].as_table()) {
                if (auto v = read_str(build, "entry"))
                    cfg.entry = *v;
                if (auto v = read_str(build, "output"))
                    cfg.output = *v;
                if (auto v = read_str(build, "mode"))
                    cfg.mode = *v;
                if (auto *ov = build->get("opt_level")) {
                    if (auto v = ov->value<int>())
                        cfg.opt_level = *v;
                }
            }

            if (auto *paths = tbl["paths"].as_table()) {
                if (auto *src = paths->get("src_dir")) {
                    if (auto *arr = src->as_array()) {
                        for (auto &elem : *arr)
                            if (auto v = elem.value<std::string>())
                                cfg.src_dirs.push_back(*v);
                    } else if (auto v = src->value<std::string>()) {
                        cfg.src_dirs.push_back(*v);
                    }
                }
                if (auto v = read_str(paths, "asset_dir"))
                    cfg.asset_dir = *v;
                if (auto v = read_str(paths, "bin_dir"))
                    cfg.bin_dir = *v;
                if (auto v = read_str(paths, "mod_dir"))
                    cfg.mod_dir = *v;
                if (auto v = read_str(paths, "docs_dir"))
                    cfg.docs_dir = *v;
                if (auto v = read_str(paths, "test_dir"))
                    cfg.test_dir = *v;
            }

            return cfg;
        };

#if TOML_EXCEPTIONS
        try {
            return process(toml::parse_file(toml_path));
        } catch (...) {
            return std::nullopt;
        }
#else
        auto result = toml::parse_file(toml_path);
        if (!result)
            return std::nullopt;
        return process(result);
#endif
    }
};

} // namespace zith::cli
