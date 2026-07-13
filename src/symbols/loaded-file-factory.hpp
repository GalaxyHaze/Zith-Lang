#pragma once

#include "dep-graph.hpp"
#include "import-types.hpp"
#include "memory/arena.hpp"
#include "memory/string-interner.hpp"
#include "module-loader.hpp"

#include <string>
#include <string_view>

namespace zith::symbols::loaded_file_factory {

auto makeImportedFile(memory::Arena &arena, const std::string &import_key, const std::string &ns,
                      bool is_from, bool is_export, const std::string &alias,
                      int32_t import_depth, LoadedModule module, ModuleDependencies deps,
                      const memory::DynArray<ast::ImportSymbol> &import_symbols) -> LoadedFile;

auto makeHeaderFile(memory::Arena &arena, memory::StringInterner &interner,
                    const std::string &full_path, const std::string &import_key,
                    const std::string &ns, bool is_from, bool is_export,
                    const std::string &alias, int32_t import_depth) -> LoadedFile;

auto makeAssetFile(memory::Arena &arena, memory::StringInterner &interner,
                   const std::string &full_path, const std::string &import_key,
                   std::string_view alias) -> LoadedFile;

} // namespace zith::symbols::loaded_file_factory
