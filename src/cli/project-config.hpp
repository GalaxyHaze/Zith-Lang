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
    std::string output  = "a.out";
    std::string mode    = "debug";
    int opt_level       = 0;

    // [paths]
    std::vector<std::string> src_dirs;
    std::string bin_dir   = "target";
    std::string mod_dir   = ".zmodules";
    std::string docs_dir  = "docs";
    std::string test_dir  = "test";
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

        try {
            auto tbl = toml::parse_file(toml_path);
            ProjectConfig cfg;

            if (auto *proj = tbl["project"].as_table()) {
                if (auto v = proj->get("name")->value<std::string>())         cfg.name = *v;
                if (auto v = proj->get("version")->value<std::string>())      cfg.version = *v;
                if (auto v = proj->get("description")->value<std::string>())  cfg.description = *v;
                if (auto v = proj->get("authors")->value<std::string>())      cfg.authors = *v;
                if (auto v = proj->get("license")->value<std::string>())      cfg.license = *v;
                if (auto v = proj->get("homepage")->value<std::string>())     cfg.homepage = *v;
            }

            if (auto *build = tbl["build"].as_table()) {
                if (auto v = build->get("entry")->value<std::string>())       cfg.entry = *v;
                if (auto v = build->get("output")->value<std::string>())      cfg.output = *v;
                if (auto v = build->get("mode")->value<std::string>())        cfg.mode = *v;
                if (auto v = build->get("opt_level")->value<int>())           cfg.opt_level = *v;
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
                if (auto v = paths->get("asset_dir")->value<std::string>())   cfg.asset_dir = *v;
                if (auto v = paths->get("bin_dir")->value<std::string>())     cfg.bin_dir = *v;
                if (auto v = paths->get("mod_dir")->value<std::string>())     cfg.mod_dir = *v;
                if (auto v = paths->get("docs_dir")->value<std::string>())    cfg.docs_dir = *v;
                if (auto v = paths->get("test_dir")->value<std::string>())    cfg.test_dir = *v;
            }

            return cfg;
        } catch (...) {
            return std::nullopt;
        }
    }
};

} // namespace zith::cli
