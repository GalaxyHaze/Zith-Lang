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

namespace zith::cli {
    struct Options;
}

namespace zith::import {

    class ImportManager {
    public:
        struct LoadedFile {
            std::string import_key;
            std::string ns;
            bool is_from = false;
            memory::FileId file_id;
            SymbolTable symbols;
            ast::AstBuilder *builder;
            ast::ProgramNode program;
            memory::DynArray<SymId> public_syms;
            memory::DynArray<SymId> module_syms;
        };

        ImportManager(memory::Arena &arena,
                      const cli::Options &opts,
                      diagnostics::DiagnosticEngine &diags);

        auto resolve(const memory::DynArray<std::string_view> &path,
                     bool is_from = false)
            -> memory::Result<size_t>;

        void mergeInto(SymbolTable &main_syms, int32_t from_depth = 0);

        const LoadedFile &get(size_t idx) const;
        bool isLoaded(std::string_view path) const;

    private:
        memory::Arena &arena_;
        const cli::Options &opts_;
        diagnostics::DiagnosticEngine &diags_;
        std::unordered_map<std::string, size_t> index_by_path_;
        memory::DynArray<LoadedFile> files_;
    };

} // namespace zith::import
