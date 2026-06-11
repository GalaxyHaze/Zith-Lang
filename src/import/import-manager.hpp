#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/result.hpp"
#include "memory/source-map.hpp"
#include "import/symbol-table.hpp"
#include "ast/ast-builder.hpp"
#include "diagnostics/diagnostic-engine.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace zith::import {

    class ImportManager {
    public:
        struct LoadedFile {
            std::string import_key;
            std::string ns;
            bool is_from = false;
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
        };

        ImportManager(memory::Arena &arena,
                      memory::SourceMap &source_map,
                      diagnostics::DiagnosticEngine &diags,
                      std::vector<std::string> visible_roots = {});

        auto resolve(const memory::DynArray<std::string_view> &path,
                     bool is_from = false,
                     bool is_export = false,
                     std::string_view alias = {},
                     int32_t import_depth = 1,
                     std::string_view source_file = {})
            -> memory::Result<size_t>;

        void mergeInto(SymbolTable &main_syms, int32_t from_depth = 0);

        const LoadedFile &get(size_t idx) const;
        bool isLoaded(std::string_view path) const;

    private:
        struct DirEntry {
            std::string full_path;
            std::string relative_stem;
            int depth;
        };

        struct ResolveGuard {
            std::unordered_set<std::string> &set;
            std::string key;
            ~ResolveGuard() { set.erase(key); }
        };

        memory::Arena &arena_;
        memory::SourceMap &source_map_;
        diagnostics::DiagnosticEngine &diags_;
        std::vector<std::string> visible_roots_;
        std::unordered_map<std::string, size_t> index_by_path_;
        std::unordered_set<std::string> resolving_;
        memory::DynArray<LoadedFile> files_;

        auto find_file(const std::string &import_key, std::string_view source_file) const
            -> memory::Optional<std::string>;

        auto resolve_file(const std::string &full_path,
                          const std::string &import_key,
                          const std::string &ns,
                          bool is_from,
                          bool is_export,
                          const std::string &alias,
                          int32_t import_depth)
            -> memory::Result<size_t>;

        auto resolve_directory(const std::string &import_key,
                               const std::string &dir_path,
                               const memory::DynArray<std::string_view> &path,
                               bool is_from,
                               bool is_export,
                               const std::string &alias,
                               int32_t import_depth)
            -> memory::Result<size_t>;

        static void collect_dir_files(const std::string &dir_path,
                                        const std::string &base_dir,
                                        int max_depth,
                                        int current_depth,
                                        std::vector<DirEntry> &out);
    };

} // namespace zith::import
