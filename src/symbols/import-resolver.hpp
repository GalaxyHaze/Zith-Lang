#pragma once

#include "memory/dyn-array.hpp"
#include "memory/optional.hpp"
#include "memory/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zith::symbols::import_resolver {

struct DirEntry {
    std::string full_path;
    std::string relative_stem;
    int depth;
};

struct DirectoryImport {
    std::string full_path;
    std::string import_key;
    std::string ns;
};

auto computeNamespace(const memory::DynArray<std::string_view> &path, std::string_view alias)
    -> std::string;

auto findFile(const std::string &import_key, std::string_view source_file,
              const std::vector<std::string> &visible_roots) -> memory::Optional<std::string>;

auto findAssetFile(const std::string &import_key, const std::vector<std::string> &asset_roots)
    -> memory::Optional<std::string>;

auto planDirectoryImports(const std::string &import_key, const std::string &dir_path,
                          const memory::DynArray<std::string_view> &path, std::string_view alias,
                          bool is_from, int32_t import_depth)
    -> memory::Result<std::vector<DirectoryImport>>;

void collectDirFiles(const std::string &dir_path, const std::string &base_dir, int max_depth,
                     int current_depth, std::vector<DirEntry> &out);

} // namespace zith::symbols::import_resolver
