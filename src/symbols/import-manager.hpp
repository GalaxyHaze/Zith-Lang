#pragma once

#include "ast/ast-builder.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/flat-map.hpp"
#include "memory/optional.hpp"
#include "memory/result.hpp"
#include "memory/source-map.hpp"
#include "symbols/symbol-table.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace zith::symbols {

class ImportManager {
public:
    struct LoadedFile {
        std::string import_key;
        std::string ns;
        bool is_from   = false;
        bool is_export = false;
        std::string alias;
        int32_t import_depth = 1;
        memory::FileId file_id;
        SymbolTable symbols;
        ast::AstBuilder *builder;
        ast::ProgramNode program;
        memory::DynArray<SymId> public_syms;
        memory::DynArray<SymId> module_syms;
        memory::DynArray<size_t> re_exported_files;
        memory::DynArray<ast::ImportSymbol> import_symbols;
        bool is_asset      = false;
        bool is_c_header   = false;
        std::string header_path;
    };

    struct SymOrigin {
        size_t file_idx;
        SymId local_sym;
    };

    ImportManager(memory::Arena &arena, memory::StringInterner &interner,
                  memory::SourceMap &source_map, diagnostics::DiagnosticEngine &diags,
                  std::vector<std::string> visible_roots = {});

    auto resolve(const memory::DynArray<std::string_view> &path,
                 const memory::DynArray<ast::ImportSymbol> &symbols,
                 bool is_from = false,
                 bool is_export = false, bool is_asset = false,
                 std::string_view alias = {}, int32_t import_depth = 1,
                 std::string_view source_file = {}) -> memory::Result<size_t>;

    void mergeInto(SymbolTable &main_syms, int32_t from_depth = 0);

    const LoadedFile &get(size_t idx) const;
    size_t fileCount() const noexcept { return files_.size(); }
    bool isLoaded(std::string_view path) const;
    memory::Optional<SymOrigin> originOf(SymId main_sym) const;
    void setVisibleRoots(std::vector<std::string> roots);
    void setAssetRoots(std::vector<std::string> roots);

private:
    struct DirEntry {
        std::string full_path;
        std::string relative_stem;
        int depth;
    };

    struct ResolveGuard {
        memory::FlatMap<std::string, char> &set;
        std::string key;
        ~ResolveGuard() {
            set.erase(key);
        }
    };

    memory::Arena &arena_;
    memory::StringInterner &interner_;
    memory::SourceMap &source_map_;
    diagnostics::DiagnosticEngine &diags_;
    std::vector<std::string> visible_roots_;
    std::vector<std::string> asset_roots_;
    memory::FlatMap<std::string, size_t> index_by_path_;
    memory::FlatMap<std::string, char> resolving_;
    memory::DynArray<LoadedFile> files_;
    memory::DynArray<SymOrigin> sym_origins_;

    auto find_file(const std::string &import_key, std::string_view source_file) const
        -> memory::Optional<std::string>;

    auto resolve_file(const std::string &full_path, const std::string &import_key,
                      const std::string &ns, bool is_from, bool is_export,
                      const std::string &alias,
                      int32_t import_depth,
                      const memory::DynArray<ast::ImportSymbol> &symbols,
                      bool is_asset = false) -> memory::Result<size_t>;

    auto resolve_directory(const std::string &import_key, const std::string &dir_path,
                           const memory::DynArray<std::string_view> &path, bool is_from,
                           bool is_export, const std::string &alias, int32_t import_depth)
        -> memory::Result<size_t>;

    auto resolve_asset_file(const std::string &full_path, const std::string &import_key,
                            const std::string &alias)
        -> memory::Result<size_t>;

    static void collect_dir_files(const std::string &dir_path, const std::string &base_dir,
                                  int max_depth, int current_depth, std::vector<DirEntry> &out);
};

} // namespace zith::symbols
