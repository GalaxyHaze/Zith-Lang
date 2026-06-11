#include "import-manager.hpp"
#include "diagnostics/error-codes.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace zith::import {
namespace fs = std::filesystem;

ImportManager::ImportManager(memory::Arena &arena,
                              memory::SourceMap &source_map,
                              diagnostics::DiagnosticEngine &diags,
                              std::vector<std::string> visible_roots)
    : arena_(arena), source_map_(source_map), diags_(diags),
      visible_roots_(std::move(visible_roots)), files_(arena) {}

bool ImportManager::isLoaded(std::string_view path) const {
    auto key = std::string(path);
    return index_by_path_.contains(key) || resolving_.contains(key);
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

static std::string_view last_segment(const std::string &path) {
    auto pos = path.rfind('/');
    if (pos == std::string::npos)
        return path;
    return {path.c_str() + pos + 1, path.size() - pos - 1};
}

static auto arena_str(memory::Arena &arena, const std::string &s) -> std::string_view {
    auto *buf = static_cast<char*>(arena.alloc(s.size() + 1));
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    return {buf, s.size()};
}

static bool path_under_root(const fs::path &p, const fs::path &root) {
    auto r = root.lexically_normal().string();
    if (r.empty()) return false;
    if (r.back() != '/') r += '/';
    auto f = p.lexically_normal().string();
    return f.size() >= r.size() && f.substr(0, r.size()) == r;
}

auto ImportManager::find_file(const std::string &import_key, std::string_view source_file) const
    -> memory::Optional<std::string>
{
    auto check = [](const fs::path &p) { return fs::is_regular_file(p); };

    // ── Relative path (../ hatch) ──────────────────────────────────
    if (import_key.size() >= 2 && import_key[0] == '.' && import_key[1] == '.') {
        if (source_file.empty())
            return {};
        fs::path src(source_file);
        auto base = src.parent_path();
        auto abs = fs::weakly_canonical(base / import_key);
        // Try .zith
        {
            auto p = abs.string() + ".zith";
            if (check(p)) {
                for (auto &root : visible_roots_)
                    if (path_under_root(fs::path(p), fs::path(root)))
                        return p;
                return {};
            }
        }
        // Try /mod.zith
        {
            auto mod_p = (abs / "mod.zith").string();
            if (check(mod_p)) {
                for (auto &root : visible_roots_)
                    if (path_under_root(fs::path(mod_p), fs::path(root)))
                        return mod_p;
                return {};
            }
        }
        // Try as directory
        if (fs::is_directory(abs)) {
            for (auto &root : visible_roots_)
                if (path_under_root(abs, fs::path(root)))
                    return abs.string();
            return {};
        }
        return {};
    }

    // ── Search visible roots ───────────────────────────────────────
    for (auto &root : visible_roots_) {
        auto base = fs::path(root) / import_key;

        // 1. Exact file match
        auto p = base.string() + ".zith";
        if (check(p)) return p;

        // 2. Directory with mod.zith entry
        auto mod_p = (base / "mod.zith").string();
        if (check(mod_p)) return mod_p;

        // 3. Directory (no mod.zith) — already under this root since base is root-relative
        if (fs::is_directory(base))
            return base.string();
    }

    return {};
}

auto ImportManager::resolve_file(const std::string &full_path,
                                  const std::string &import_key,
                                  const std::string &ns,
                                  bool is_from,
                                  bool is_export,
                                  const std::string &alias,
                                  int32_t import_depth)
    -> memory::Result<size_t>
{
    if (auto it = index_by_path_.find(import_key); it != index_by_path_.end())
        return it->second;

    auto file_result = source_map_.loadFile(full_path);
    if (!file_result)
        return memory::Error{"failed to load '" + full_path + "'"};

    auto file_id = file_result.value();
    auto token_result = lexer::tokenize(source_map_, file_id, diags_);
    if (!token_result)
        return memory::Error{"failed to tokenize '" + full_path + "'"};

    lexer::TokenStream tokens = std::move(token_result.value());
    auto *builder = arena_.make<ast::AstBuilder>(arena_);
    SymbolTable syms(arena_);
    parser::Parser parser{&tokens, builder, &diags_};
    auto scan_result = parser::scan(parser, syms);
    parser.expandBodies(scan_result);

    memory::DynArray<SymId> public_syms{arena_};
    memory::DynArray<SymId> module_syms{arena_};

    for (SymId id = 0; id < static_cast<SymId>(syms.symbolCount()); ++id) {
        auto &data = syms.get(id);
        if (data.visibility == SymbolVisibility::Public)
            public_syms.push(id);
        else if (data.visibility == SymbolVisibility::Module)
            module_syms.push(id);
    }

    // ── Handle re-exports ──────────────────────────────────────────
    memory::DynArray<size_t> re_exported_files{arena_};
    for (auto decl_id : parser.program.decls) {
        auto &decl = builder->getDecl(decl_id);
        if (auto *import = std::get_if<ast::ImportNode>(&decl)) {
            if (import->is_export) {
                auto re_res = resolve(import->path, /*is_from=*/true,
                                      /*is_export=*/true, import->alias,
                                      import->import_depth, full_path);
                if (re_res) {
                    re_exported_files.push(re_res.value());
                } else {
                    auto key = join_path(import->path, '/');
                    diags_.report(diagnostics::Severity::Warning,
                                  diagnostics::err::ImportError,
                                  "re-export of '" + key + "' failed: " + re_res.error().msg, {});
                }
            }
        }
    }

    size_t idx = files_.size();
    files_.push(LoadedFile{
        import_key,
        ns,
        is_from,
        is_export,
        alias,
        import_depth,
        file_id,
        std::move(syms),
        builder,
        std::move(parser.program),
        std::move(public_syms),
        std::move(module_syms),
        std::move(re_exported_files),
    });
    index_by_path_[import_key] = idx;
    return idx;
}

void ImportManager::collect_dir_files(const std::string &dir_path,
                                       const std::string &base_dir,
                                       int max_depth,
                                       int current_depth,
                                       std::vector<DirEntry> &out)
{
    if (max_depth != -1 && current_depth > max_depth)
        return;

    fs::path dir(dir_path);
    if (!fs::is_directory(dir))
        return;

    for (auto &entry : fs::directory_iterator(dir)) {
        auto &p = entry.path();
        if (entry.is_regular_file() && p.extension() == ".zith") {
            auto rel = fs::relative(p, fs::path(base_dir)).string();
            if (rel.size() >= 5)
                rel = rel.substr(0, rel.size() - 5);
            out.push_back({p.string(), rel, current_depth});
        } else if (entry.is_directory()) {
            collect_dir_files(p.string(), base_dir, max_depth, current_depth + 1, out);
        }
    }
}

auto ImportManager::resolve_directory(const std::string &import_key,
                                       const std::string &dir_path,
                                       const memory::DynArray<std::string_view> &path,
                                       bool is_from,
                                       bool is_export,
                                       const std::string &alias,
                                       int32_t import_depth)
    -> memory::Result<size_t>
{
    std::vector<DirEntry> files;
    collect_dir_files(dir_path, dir_path, import_depth, 1, files);

    if (files.empty())
        return memory::Error{"directory '" + dir_path + "' contains no .zith files"};

    size_t first_idx = SIZE_MAX;

    for (auto &entry : files) {
        auto sub_key = import_key + "/" + entry.relative_stem;

        if (index_by_path_.count(sub_key))
            continue;

        // Build ns: strip .zith, replace / with .
        std::string ns;
        if (!alias.empty()) {
            ns = alias;
            for (char c : entry.relative_stem) {
                ns += (c == '/') ? '.' : c;
            }
        } else if (is_from) {
            for (char c : entry.relative_stem) {
                ns += (c == '/') ? '.' : c;
            }
        } else {
            for (size_t i = 0; i < path.size(); ++i) {
                if (i > 0 || !ns.empty()) ns += '.';
                ns.append(path[i].data(), path[i].size());
            }
            if (!ns.empty()) ns += '.';
            for (char c : entry.relative_stem) {
                ns += (c == '/') ? '.' : c;
            }
        }

        auto res = resolve_file(entry.full_path, sub_key, ns,
                                is_from, is_export, alias, import_depth);
        if (!res)
            return std::move(res.error());

        if (first_idx == SIZE_MAX)
            first_idx = res.value();
    }

    index_by_path_[import_key] = first_idx;
    return first_idx;
}

auto ImportManager::resolve(const memory::DynArray<std::string_view> &path,
                             bool is_from,
                             bool is_export,
                             std::string_view alias,
                             int32_t import_depth,
                             std::string_view source_file)
    -> memory::Result<size_t> {

    auto import_key = join_path(path, '/');

    // ── Dedup ───────────────────────────────────────────────────────
    if (auto it = index_by_path_.find(import_key); it != index_by_path_.end())
        return it->second;

    // ── Cycle detection ────────────────────────────────────────────
    if (resolving_.count(import_key))
        return memory::Error{"circular import detected: '" + import_key + "'"};

    resolving_.insert(import_key);
    ResolveGuard guard{resolving_, import_key};

    auto file_path = find_file(import_key, source_file);
    if (!file_path) {
        std::string msg = "could not resolve import '" + import_key + "'";
        if (!visible_roots_.empty()) {
            msg += " — searched roots:";
            for (auto &r : visible_roots_)
                msg += "\n    " + r;
        }
        if (source_file.empty())
            msg += "\n    (no source file context for relative resolution)";
        else
            msg += "\n    (relative to: " + std::string(source_file) + ")";
        return memory::Error{std::move(msg)};
    }

    // ── Directory import ───────────────────────────────────────────
    if (fs::is_directory(*file_path))
        return resolve_directory(import_key, *file_path, path,
                                 is_from, is_export, std::string(alias),
                                 import_depth);

    // ── Single file resolution ─────────────────────────────────────
    auto ns = [&]() -> std::string {
        if (!alias.empty())
            return std::string(alias);
        std::vector<std::string_view> parts;
        for (auto &seg : path) {
            if (seg == ".." && !parts.empty())
                parts.pop_back();
            else if (seg != ".." && seg != ".")
                parts.push_back(seg);
        }
        std::string result;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) result += '.';
            result.append(parts[i].data(), parts[i].size());
        }
        return result;
    }();

    return resolve_file(*file_path, import_key, ns,
                        is_from, is_export, std::string(alias),
                        import_depth);
}

void ImportManager::mergeInto(SymbolTable &main_syms, int32_t from_depth) {
    int32_t call_depth = from_depth + 1;

    for (auto &file : files_) {
        auto prefix = file.ns + ".";
        auto ls = last_segment(file.import_key);

        // ── Merge own symbols ──────────────────────────────────────
        auto merge_sym = [&](SymId sid, bool is_module, int32_t mod_depth) {
            auto &data = file.symbols.get(sid);
            if (is_module && data.mod_depth >= 0 && call_depth != data.mod_depth)
                return;
            auto vis = is_module ? SymbolVisibility::Module : SymbolVisibility::Public;
            auto depth = is_module ? data.mod_depth : 0;
            auto declare_or_diag = [&](std::string_view name, SymbolVisibility v, int32_t d) {
                if (main_syms.lookupInScope(name, kRootScope) != kInvalidSym)
                    diags_.report(diagnostics::Severity::Error,
                                  diagnostics::err::DuplicateDecl,
                                  "duplicate symbol '" + std::string(name) + "'", {});
                else
                    main_syms.declare(name, v, d);
            };
            if (file.is_from) {
                declare_or_diag(arena_str(arena_, std::string(data.name)), vis, depth);
                auto qualified = std::string(ls) + "." + std::string(data.name);
                declare_or_diag(arena_str(arena_, qualified), vis, depth);
            } else {
                std::string qualified = prefix + std::string(data.name);
                declare_or_diag(arena_str(arena_, qualified), vis, depth);
            }
        };

        for (auto sid : file.public_syms)
            merge_sym(sid, false, 0);
        for (auto sid : file.module_syms)
            merge_sym(sid, true, call_depth);

        // ── Merge re-exported files ────────────────────────────────
        std::unordered_set<size_t> visited;
        auto declare_or_diag = [&](std::string_view name, SymbolVisibility v, int32_t d) {
            if (main_syms.lookupInScope(name, kRootScope) != kInvalidSym)
                diags_.report(diagnostics::Severity::Error,
                              diagnostics::err::DuplicateDecl,
                              "duplicate symbol '" + std::string(name) + "'", {});
            else
                main_syms.declare(name, v, d);
        };
        auto process_re_exports = [&](auto &self, size_t file_idx) -> void {
            if (!visited.insert(file_idx).second) return;
            auto &ref = files_[file_idx];
            for (auto sid : ref.public_syms) {
                auto &data = ref.symbols.get(sid);
                if (file.is_from) {
                    declare_or_diag(arena_str(arena_, std::string(data.name)),
                                    SymbolVisibility::Public, 0);
                    auto qualified = std::string(ls) + "." + std::string(data.name);
                    declare_or_diag(arena_str(arena_, qualified),
                                    SymbolVisibility::Public, 0);
                } else {
                    std::string qualified = prefix + std::string(data.name);
                    declare_or_diag(arena_str(arena_, qualified),
                                    SymbolVisibility::Public, 0);
                }
            }
            for (auto sid : ref.module_syms) {
                auto &data = ref.symbols.get(sid);
                if (data.mod_depth >= 0 && call_depth != data.mod_depth)
                    continue;
                if (file.is_from) {
                    declare_or_diag(arena_str(arena_, std::string(data.name)),
                                    SymbolVisibility::Module, data.mod_depth);
                    auto qualified = std::string(ls) + "." + std::string(data.name);
                    declare_or_diag(arena_str(arena_, qualified),
                                    SymbolVisibility::Module, data.mod_depth);
                } else {
                    std::string qualified = prefix + std::string(data.name);
                    declare_or_diag(arena_str(arena_, qualified),
                                    SymbolVisibility::Module, data.mod_depth);
                }
            }
            for (auto re_idx : ref.re_exported_files)
                self(self, re_idx);
        };

        for (auto re_idx : file.re_exported_files)
            process_re_exports(process_re_exports, re_idx);
    }
}

} // namespace zith::import
