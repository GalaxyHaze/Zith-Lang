#pragma once

#include "memory/dyn-array.hpp"
#include <string>

namespace zith {

struct ProjectConfig {
    // [build]
    std::string entry;
    std::string output;
    std::string mode;
    int opt_level = 0;

    // [paths]
    memory::DynArray<std::string> srcDirs;
    std::string binDir;
    std::string modDir;
    std::string docsDir;
    std::string testDir;
    std::string assetDir;

    // [project]
    std::string name;
    std::string version;
    std::string description;
    std::string authors;
    std::string license;
    std::string homepage;

    explicit ProjectConfig(memory::Arena &arena) : srcDirs(arena) {}
};

} // namespace zith
