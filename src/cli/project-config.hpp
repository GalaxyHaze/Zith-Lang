#pragma once

#include "memory/dyn-array.hpp"
#include "memory/string-interner.hpp"
#include <string>

namespace zith {

struct ProjectConfig {
    static constexpr memory::InternedId kUnset = static_cast<memory::InternedId>(-1);

    // [build]
    memory::InternedId entry  = kUnset;
    memory::InternedId output = kUnset;
    memory::InternedId mode   = kUnset;
    int opt_level = 0;

    // [paths]
    memory::DynArray<memory::InternedId> srcDirs;
    memory::InternedId binDir    = kUnset;
    memory::InternedId modDir    = kUnset;
    memory::InternedId docsDir   = kUnset;
    memory::InternedId testDir   = kUnset;
    memory::InternedId assetDir  = kUnset;

    memory::InternedId projectRoot = kUnset; // directory containing ZithProject.toml

    // [project]
    memory::InternedId name        = kUnset;
    memory::InternedId version     = kUnset;
    memory::InternedId description = kUnset;
    memory::InternedId authors     = kUnset;
    memory::InternedId license     = kUnset;
    memory::InternedId homepage    = kUnset;

    memory::StringInterner *stringPool = nullptr;

    explicit ProjectConfig(memory::Arena &arena) : srcDirs(arena) {}
};

} // namespace zith
