#pragma once

#include <string>
#include <vector>

struct ZithProject {
    std::string name = "project";
    std::string version = "0.1.0";
    std::string description;
    std::string authors;
    std::string license;
    std::string homepage;

    std::string entry = "src/main.zith";
    std::string output = "bin/project";
    std::string mode = "debug";
    std::string target_triple;
    std::string edition = "2024";

    std::string src_dir = "src";
    std::string bin_dir = "bin";
    std::string lib_dir = "lib";
    std::string docs_dir = "docs";
    std::string test_dir = "examples";
    std::string cache_dir = ".zith_cache";

    std::vector<std::string> include_dirs;
    std::vector<std::string> lib_paths;
    std::vector<std::string> link_libs;
    std::vector<std::string> link_flags;

    std::vector<std::string> features;
    std::vector<std::string> dependencies;

    bool emit_ir = false;
    bool emit_asm = false;
    bool strip_debug = false;
    bool lto = false;
    int opt_level = 0;
    int debug_level = 2;

    static void build_import_roots(const std::vector<std::string> &extra_dirs,
                                   std::vector<const char *> &roots_out,
                                   size_t &count_out);
};

bool try_load_project(ZithProject &proj);
bool try_load_project_from_path(ZithProject &proj,
                                const std::string &project_file,
                                std::vector<std::string> *warnings = nullptr,
                                std::vector<std::string> *errors = nullptr);
