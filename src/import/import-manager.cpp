#include "import-manager.hpp"
#include "cli/options.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <cstring>
#include <filesystem>
#include <string>

namespace zith::import {
namespace fs = std::filesystem;

ImportManager::ImportManager(memory::Arena &arena,
                              const cli::Options &opts,
                              diagnostics::DiagnosticEngine &diags)
    : arena_(arena), opts_(opts), diags_(diags), files_(arena) {}

bool ImportManager::isLoaded(std::string_view path) const {
    return index_by_path_.contains(std::string(path));
}

const ImportManager::LoadedFile &ImportManager::get(size_t idx) const {
    return files_[idx];
}

static std::string join_path(const memory::DynArray<std::string_view> &path, char sep) {
    std::string result;
    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) result += sep;
        result.append(path[i].data(), path[i].size());
    }
    return result;
}

static auto find_file(const std::string &import_key,
                       const memory::DynArray<std::string> &include_dirs)
    -> memory::Optional<std::string> {

    auto check = [](const std::string &path) -> bool {
        return fs::is_regular_file(path);
    };

    for (auto &dir : include_dirs) {
        auto path = (fs::path(dir) / import_key).string() + ".zith";
        if (check(path)) return path;

        auto mod_path = (fs::path(dir) / import_key / "mod.zith").string();
        if (check(mod_path)) return mod_path;
    }

    {
        auto path = import_key + ".zith";
        if (check(path)) return path;

        auto mod_path = import_key + "/mod.zith";
        if (check(mod_path)) return mod_path;
    }

    return {};
}

static auto arena_str(memory::Arena &arena, const std::string &s) -> std::string_view {
    auto *buf = static_cast<char*>(arena.alloc(s.size() + 1));
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    return {buf, s.size()};
}

auto ImportManager::resolve(const memory::DynArray<std::string_view> &path,
                             bool is_from)
    -> memory::Result<size_t> {

    auto import_key = join_path(path, '/');

    if (auto it = index_by_path_.find(import_key); it != index_by_path_.end())
        return it->second;

    auto file_path = find_file(import_key, opts_.include_dirs);
    if (!file_path)
        return memory::Error{"could not resolve import '" + import_key + "'"};

    auto file_result = memory::SourceMap::load_file(*file_path);
    if (!file_result)
        return memory::Error{"failed to load '" + *file_path + "'"};

    auto file_id = file_result.value();
    auto token_result = lexer::tokenize(file_id, diags_);
    if (!token_result)
        return memory::Error{"failed to tokenize '" + *file_path + "'"};

    lexer::TokenStream tokens = std::move(token_result.value());
    auto *builder = arena_.make<ast::AstBuilder>(arena_);
    SymbolTable syms(arena_);
    parser::Parser parser{&tokens, builder, &diags_};
    auto scan_result = parser::scan(parser, syms);
    parser.expandBodies(scan_result);

    auto ns = join_path(path, '.');

    memory::DynArray<SymId> public_syms{arena_};
    memory::DynArray<SymId> module_syms{arena_};

    for (SymId id = 0; id < static_cast<SymId>(syms.symbolCount()); ++id) {
        auto &data = syms.get(id);
        if (data.visibility == SymbolVisibility::Public)
            public_syms.push(id);
        else if (data.visibility == SymbolVisibility::Module)
            module_syms.push(id);
    }

    size_t idx = files_.size();
    files_.push(LoadedFile{
        std::move(import_key),
        std::move(ns),
        is_from,
        file_id,
        std::move(syms),
        builder,
        std::move(parser.program),
        std::move(public_syms),
        std::move(module_syms),
    });
    index_by_path_[files_[idx].import_key] = idx;
    return idx;
}

void ImportManager::mergeInto(SymbolTable &main_syms, int32_t from_depth) {
    int32_t call_depth = from_depth + 1;

    for (auto &file : files_) {
        auto prefix = file.ns + ".";

        for (auto sid : file.public_syms) {
            auto &data = file.symbols.get(sid);
            std::string qualified;
            if (file.is_from)
                qualified = std::string(data.name);
            else
                qualified = prefix + std::string(data.name);
            main_syms.declare(arena_str(arena_, qualified), SymbolVisibility::Public);
        }

        for (auto sid : file.module_syms) {
            auto &data = file.symbols.get(sid);
            if (data.mod_depth >= 0 && call_depth > data.mod_depth)
                continue;
            std::string qualified;
            if (file.is_from)
                qualified = std::string(data.name);
            else
                qualified = prefix + std::string(data.name);
            main_syms.declare(arena_str(arena_, qualified), SymbolVisibility::Module, data.mod_depth);
        }
    }
}

} // namespace zith::import
