#include "loaded-file-factory.hpp"

namespace zith::symbols::loaded_file_factory {

namespace {

auto copyImportSymbols(memory::Arena &arena,
                       const memory::DynArray<ast::ImportSymbol> &import_symbols)
    -> memory::DynArray<ast::ImportSymbol> {
    memory::DynArray<ast::ImportSymbol> copied{arena};
    for (auto &symbol : import_symbols)
        copied.push(symbol);
    return copied;
}

} // namespace

auto makeImportedFile(memory::Arena &arena, const std::string &import_key, const std::string &ns,
                      bool is_from, bool is_export, const std::string &alias, int32_t import_depth,
                      LoadedModule module, ModuleDependencies deps,
                      const memory::DynArray<ast::ImportSymbol> &import_symbols) -> LoadedFile {
    return LoadedFile{
        import_key,
        ns,
        is_from,
        is_export,
        alias,
        import_depth,
        module.file_id,
        std::move(module.symbols),
        module.builder,
        std::move(module.program),
        std::move(module.public_syms),
        std::move(module.module_syms),
        std::move(deps.re_exported_files),
        std::move(deps.dep_files),
        copyImportSymbols(arena, import_symbols),
        false,
        false,
        {},
    };
}

auto makeHeaderFile(memory::Arena &arena, memory::StringInterner &interner,
                    const std::string &full_path, const std::string &import_key,
                    const std::string &ns, bool is_from, bool is_export, const std::string &alias,
                    int32_t import_depth) -> LoadedFile {
    memory::DynArray<SymId> empty_pub{arena};
    memory::DynArray<SymId> empty_mod{arena};
    memory::DynArray<size_t> empty_re{arena};
    memory::DynArray<size_t> empty_deps{arena};
    memory::DynArray<ast::ImportSymbol> empty_imports{arena};

    return LoadedFile{
        import_key,
        ns,
        is_from,
        is_export,
        alias,
        import_depth,
        memory::FileId(0),
        SymbolTable(arena, &interner),
        nullptr,
        ast::ProgramNode{arena},
        std::move(empty_pub),
        std::move(empty_mod),
        std::move(empty_re),
        std::move(empty_deps),
        std::move(empty_imports),
        false,
        true,
        full_path,
    };
}

auto makeAssetFile(memory::Arena &arena, memory::StringInterner &interner,
                   const std::string &full_path, const std::string &import_key,
                   std::string_view alias) -> LoadedFile {
    auto symbols = SymbolTable(arena, &interner);
    auto sym_id  = symbols.declare(alias, SymbolVisibility::Public, 0, SymKind::Asset,
                                   ast::kInvalidDecl, {}, kInvalidSym, {});

    memory::DynArray<SymId> public_syms{arena};
    public_syms.push(sym_id);
    memory::DynArray<SymId> module_syms{arena};
    memory::DynArray<size_t> re_exported_files{arena};
    memory::DynArray<size_t> dep_files{arena};
    memory::DynArray<ast::ImportSymbol> import_symbols{arena};

    return LoadedFile{
        import_key,
        std::string(alias),
        true,
        false,
        std::string(alias),
        1,
        memory::FileId(0),
        std::move(symbols),
        nullptr,
        ast::ProgramNode{arena},
        std::move(public_syms),
        std::move(module_syms),
        std::move(re_exported_files),
        std::move(dep_files),
        std::move(import_symbols),
        true,
        false,
        full_path,
    };
}

} // namespace zith::symbols::loaded_file_factory
