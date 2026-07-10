#include "import-manager.hpp"
#include "diagnostics/error-codes.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace zith::symbols {
namespace fs = std::filesystem;

ImportManager::ImportManager(memory::Arena &arena, memory::StringInterner &interner,
                             memory::SourceMap &source_map,
                             diagnostics::DiagnosticEngine &diags,
                             std::vector<std::string> visible_roots)
    : arena_(arena), interner_(interner), source_map_(source_map), diags_(diags),
      visible_roots_(std::move(visible_roots)), files_(arena), sym_origins_(arena) {}

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
        if (i > 0)
            result += sep;
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
    auto *buf = static_cast<char *>(arena.alloc(s.size() + 1));
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    return {buf, s.size()};
}

static bool path_under_root(const fs::path &p, const fs::path &root) {
    auto r = root.lexically_normal().generic_string();
    if (r.empty())
        return false;
    if (r.back() != '/')
        r += '/';
    auto f = p.lexically_normal().generic_string();
    return f.size() >= r.size() && f.substr(0, r.size()) == r;
}

auto ImportManager::find_file(const std::string &import_key, std::string_view source_file) const
    -> memory::Optional<std::string> {
    auto check = [](const fs::path &p) { return fs::is_regular_file(p); };

    // ── Relative path (../ hatch) ──────────────────────────────────
    if (import_key.size() >= 2 && import_key[0] == '.' && import_key[1] == '.') {
        if (source_file.empty())
            return {};
        fs::path src(source_file);
        auto base = src.parent_path();
        auto abs  = fs::weakly_canonical(base / import_key);
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
        // Try .h
        {
            auto p = abs.string() + ".h";
            if (check(p)) {
                for (auto &root : visible_roots_)
                    if (path_under_root(fs::path(p), fs::path(root)))
                        return p;
                return {};
            }
        }
        // Try .hpp
        {
            auto p = abs.string() + ".hpp";
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
        if (check(p))
            return p;

        // 2. .h / .hpp
        {
            auto hp = base.string() + ".h";
            if (check(hp))
                return hp;
        }
        {
            auto hpp = base.string() + ".hpp";
            if (check(hpp))
                return hpp;
        }

        // 3. Directory with mod.zith entry
        auto mod_p = (base / "mod.zith").string();
        if (check(mod_p))
            return mod_p;

        // 4. Directory (no mod.zith) — already under this root since base is root-relative
        if (fs::is_directory(base))
            return base.string();
    }

    return {};
}

auto ImportManager::resolve_file(const std::string &full_path, const std::string &import_key,
                                 const std::string &ns, bool is_from, bool is_export,
                                 const std::string &alias,
                                 int32_t import_depth,
                                 const memory::DynArray<ast::ImportSymbol> &symbols,
                                 bool is_asset)
    -> memory::Result<size_t> {
    if (auto *existing = index_by_path_.get(import_key))
        return *existing;

    // ── C/C++ header: don't tokenize, just register ───────────────
    auto ext = fs::path(full_path).extension();
    bool is_c_header = (ext == ".h" || ext == ".hpp");

    if (is_c_header || is_asset) {
        memory::DynArray<SymId> empty_pub{arena_};
        memory::DynArray<SymId> empty_mod{arena_};
        memory::DynArray<size_t> empty_re{arena_};
        auto syms_ptr = arena_.make<SymbolTable>(arena_, &interner_);

        size_t idx = files_.size();
        files_.push(LoadedFile{
            import_key,
            ns,
            is_from,
            is_export,
            alias,
            import_depth,
            memory::FileId(0),
            std::move(*syms_ptr),
            nullptr,
            ast::ProgramNode{arena_},
            std::move(empty_pub),
            std::move(empty_mod),
            std::move(empty_re),
            memory::DynArray<ast::ImportSymbol>(arena_),
            is_asset,
            is_c_header,
            full_path,
        });
        index_by_path_.insert(import_key, idx);
        return idx;
    }

    auto file_result = source_map_.loadFile(full_path);
    if (!file_result)
        return memory::Error{"failed to load '" + full_path + "'"};

    auto file_id      = file_result.value();
    auto token_result = lexer::tokenize(source_map_, arena_, file_id, diags_);
    if (!token_result)
        return memory::Error{"failed to tokenize '" + full_path + "'"};

    lexer::TokenStream tokens = std::move(token_result.value());
    auto *builder             = arena_.make<ast::AstBuilder>(arena_, interner_);
    SymbolTable syms(arena_, &interner_);
    parser::Parser parser(&tokens, builder, &diags_);
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
                auto re_res =
                    resolve(import->path, import->symbols,
                            /*is_from=*/true,
                            /*is_export=*/true, import->is_asset,
                            import->alias, import->import_depth, full_path);
                if (re_res) {
                    re_exported_files.push(re_res.value());
                } else {
                    auto key = join_path(import->path, '/');
                    diags_.report(diagnostics::Severity::Warning, diagnostics::err::ImportError,
                                  "re-export of '" + key + "' failed: " + re_res.error().msg, {});
                }
            }
        }
    }

    memory::DynArray<ast::ImportSymbol> syms_copy{arena_};
    for (auto &s : symbols)
        syms_copy.push(s);

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
        std::move(syms_copy),
        false, false, {},
    });
    index_by_path_.insert(import_key, idx);
    return idx;
}

void ImportManager::collect_dir_files(const std::string &dir_path, const std::string &base_dir,
                                      int max_depth, int current_depth,
                                      std::vector<DirEntry> &out) {
    if (max_depth != -1 && current_depth > max_depth)
        return;

    fs::path dir(dir_path);
    if (!fs::is_directory(dir))
        return;

    for (auto &entry : fs::directory_iterator(dir)) {
        auto &p = entry.path();
        auto ext = p.extension();
        if (entry.is_regular_file() && (ext == ".zith" || ext == ".h" || ext == ".hpp")) {
            auto rel = fs::relative(p, fs::path(base_dir)).generic_string();
            if (ext == ".zith") {
                if (rel.size() >= 5)
                    rel = rel.substr(0, rel.size() - 5);
            } else {
                // Keep .h/.hpp extension in the path
                auto ext_str = ext.string();
                if (rel.size() >= ext_str.size())
                    rel = rel.substr(0, rel.size() - ext_str.size());
            }
            out.push_back({p.string(), rel, current_depth});
        } else if (entry.is_directory()) {
            collect_dir_files(p.string(), base_dir, max_depth, current_depth + 1, out);
        }
    }
}

auto ImportManager::resolve_directory(const std::string &import_key, const std::string &dir_path,
                                      const memory::DynArray<std::string_view> &path, bool is_from,
                                      bool is_export, const std::string &alias,
                                      int32_t import_depth) -> memory::Result<size_t> {
    std::vector<DirEntry> files;
    collect_dir_files(dir_path, dir_path, import_depth, 1, files);

    if (files.empty())
        return memory::Error{"directory '" + dir_path + "' contains no .zith files"};

    size_t first_idx = SIZE_MAX;

    for (auto &entry : files) {
        auto sub_key = import_key + "/" + entry.relative_stem;

        if (index_by_path_.contains(sub_key))
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
                if (i > 0 || !ns.empty())
                    ns += '.';
                ns.append(path[i].data(), path[i].size());
            }
            if (!ns.empty())
                ns += '.';
            for (char c : entry.relative_stem) {
                ns += (c == '/') ? '.' : c;
            }
        }

        auto res =
            resolve_file(entry.full_path, sub_key, ns, is_from, is_export, alias, import_depth,
                         memory::DynArray<ast::ImportSymbol>(arena_), false);
        if (!res)
            return std::move(res.error());

        if (first_idx == SIZE_MAX)
            first_idx = res.value();
    }

    index_by_path_.insert(import_key, first_idx);
    return first_idx;
}

auto ImportManager::resolve_asset_file(const std::string &full_path,
                                       const std::string &import_key,
                                       const std::string &alias)
    -> memory::Result<size_t> {
    if (auto *existing = index_by_path_.get(import_key))
        return *existing;

    auto syms = SymbolTable(arena_, &interner_);
    auto sym_id = syms.declare(alias, SymbolVisibility::Public, 0, SymKind::Asset,
                               ast::kInvalidDecl, {}, kInvalidSym, {});

    memory::DynArray<SymId> pub{arena_};
    pub.push(sym_id);
    memory::DynArray<SymId> mod{arena_};
    memory::DynArray<size_t> re{arena_};
    memory::DynArray<ast::ImportSymbol> empty_syms{arena_};

    size_t idx = files_.size();
    files_.push(LoadedFile{
        import_key,
        alias,  // ns = alias
        true,   // is_from (so alias is injected unqualified)
        false,  // is_export
        alias,
        1,
        memory::FileId(0),
        std::move(syms),
        nullptr,
        ast::ProgramNode{arena_},
        std::move(pub),
        std::move(mod),
        std::move(re),
        std::move(empty_syms),
        true,  // is_asset
        false, // is_c_header
        full_path,
    });
    index_by_path_.insert(import_key, idx);
    return idx;
}

auto ImportManager::resolve(const memory::DynArray<std::string_view> &path,
                            const memory::DynArray<ast::ImportSymbol> &symbols,
                            bool is_from,
                            bool is_export, bool is_asset,
                            std::string_view alias, int32_t import_depth,
                            std::string_view source_file) -> memory::Result<size_t> {

    auto import_key = join_path(path, '/');

    // ── Dedup ───────────────────────────────────────────────────────
    if (auto *existing = index_by_path_.get(import_key))
        return *existing;

    // ── Cycle detection ────────────────────────────────────────────
    if (resolving_.contains(import_key))
        return memory::Error{"circular import detected: '" + import_key + "'"};

    resolving_.insert(import_key, char(1));
    ResolveGuard guard{resolving_, import_key};

    // ── Asset file ─────────────────────────────────────────────────
    if (is_asset) {
        for (auto &root : asset_roots_) {
            auto p = fs::path(root) / import_key;
            if (fs::is_regular_file(p))
                return resolve_asset_file(p.string(), import_key, std::string(alias));
        }
        return memory::Error{"could not resolve asset '" + import_key + "'"};
    }

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
        return resolve_directory(import_key, *file_path, path, is_from, is_export,
                                 std::string(alias), import_depth);

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
            if (i > 0)
                result += '.';
            result.append(parts[i].data(), parts[i].size());
        }
        return result;
    }();

    return resolve_file(*file_path, import_key, ns, is_from, is_export, std::string(alias),
                        import_depth, symbols, is_asset);
}

void ImportManager::mergeInto(SymbolTable &main_syms, int32_t from_depth) {
    int32_t call_depth = from_depth + 1;

    auto record_origin = [&](SymId main_sid, size_t file_idx, SymId local_sid) {
        if (main_sid >= sym_origins_.size()) {
            if (main_sid >= sym_origins_.capacity())
                sym_origins_.reserve(main_sid + 1);
            while (sym_origins_.size() <= main_sid)
                sym_origins_.push(SymOrigin{size_t(-1), kInvalidSym});
        }
        sym_origins_[main_sid] = SymOrigin{file_idx, local_sid};
    };

    std::unordered_set<std::string> from_namespaces;

    for (size_t fi = 0; fi < files_.size(); ++fi) {
        auto &file  = files_[fi];
        auto prefix = file.ns + ".";
        auto ls     = last_segment(file.import_key);

        // ── Check from-namespace collision ─────────────────────────
        if (file.is_from && !file.is_asset) {
            auto ls_str = std::string(ls);
            if (!from_namespaces.insert(ls_str).second) {
                diags_.report(diagnostics::Severity::Error, diagnostics::err::DuplicateDecl,
                              "from-namespace '" + ls_str + "' conflicts with another from-import", {});
                continue;
            }
        }

        // Build lookup map for selective imports: name -> ImportSymbol alias
        memory::FlatMap<std::string_view, std::string_view> selective_map;
        bool selective = !file.import_symbols.empty();
        for (auto &isym : file.import_symbols) {
            selective_map[isym.name] = isym.alias;
        }

        // ── Merge own symbols ──────────────────────────────────────
        auto merge_sym = [&](SymId sid, bool is_module, int32_t mod_depth) {
            auto &data = file.symbols.get(sid);
            if (is_module && data.mod_depth >= 0 && call_depth != data.mod_depth)
                return;
            auto vis             = is_module ? SymbolVisibility::Module : SymbolVisibility::Public;
            auto depth           = is_module ? data.mod_depth : 0;

            auto raw_name = interner_.lookup(data.name);

            // Selective: check if this symbol is in the import list
            if (selective) {
                auto *alias_ptr = selective_map.get(raw_name);
                if (!alias_ptr)
                    return; // not in selective list, skip
                auto alias_name = *alias_ptr;

                std::string_view declare_name;
                if (!alias_name.empty())
                    declare_name = arena_str(arena_, std::string(alias_name));
                else
                    declare_name = raw_name;

                if (main_syms.lookupInScope(declare_name, kRootScope) != kInvalidSym) {
                    diags_.report(diagnostics::Severity::Error, diagnostics::err::DuplicateDecl,
                                  "duplicate symbol '" + std::string(declare_name) + "'", {});
                    return;
                }
                auto main_id = main_syms.declare(declare_name, vis, depth, data.kind,
                                                  data.decl_id, data.span,
                                                  data.target, data.doc_span);
                record_origin(main_id, fi, sid);
                return;
            }

            // Non-selective: default injection logic
            auto declare_or_diag = [&](std::string_view name, SymbolVisibility v, int32_t d,
                                       SymId local_sid) {
                if (main_syms.lookupInScope(name, kRootScope) != kInvalidSym) {
                    std::string msg = "duplicate symbol '" + std::string(name) + "'";
                    if (file.is_from)
                        msg += " — use '" + std::string(ls) + "." + std::string(name) + "' to disambiguate";
                    diags_.report(diagnostics::Severity::Error, diagnostics::err::DuplicateDecl,
                                  std::move(msg), {});
                    return;
                }
                auto main_id = main_syms.declare(name, v, d, data.kind, data.decl_id, data.span,
                                                  data.target, data.doc_span);
                record_origin(main_id, fi, local_sid);
            };
            if (file.is_from) {
                auto sym_name_s = std::string(raw_name);
                declare_or_diag(arena_str(arena_, sym_name_s), vis, depth, sid);
                auto qualified = std::string(ls) + "." + sym_name_s;
                declare_or_diag(arena_str(arena_, qualified), vis, depth, sid);
            } else {
                std::string qualified = prefix + std::string(raw_name);
                declare_or_diag(arena_str(arena_, qualified), vis, depth, sid);
            }
        };

        // For asset files: the single symbol is the alias itself
        if (file.is_asset) {
            for (auto sid : file.public_syms)
                merge_sym(sid, false, 0);
        } else {
            for (auto sid : file.public_syms)
                merge_sym(sid, false, 0);
            for (auto sid : file.module_syms)
                merge_sym(sid, true, call_depth);
        }

        // ── Merge re-exported files ────────────────────────────────
        // Aliases from selective imports are NOT re-exported;
        // re-export only iterates over the referenced file's original symbols.
        std::unordered_set<size_t> visited;
        auto declare_re_export = [&](std::string_view name, SymbolVisibility v, int32_t d,
                                     SymKind kind, ast::DeclId decl_id, memory::Span span,
                                     SymId target, memory::Span doc_span, size_t ri, SymId ls) {
            if (main_syms.lookupInScope(name, kRootScope) != kInvalidSym) {
                diags_.report(diagnostics::Severity::Error, diagnostics::err::DuplicateDecl,
                              "duplicate symbol '" + std::string(name) + "'", {});
                return;
            }
            auto main_id = main_syms.declare(name, v, d, kind, decl_id, span, target, doc_span);
            record_origin(main_id, ri, ls);
        };
        auto process_re_exports = [&](auto &self, size_t re_idx) -> void {
            if (!visited.insert(re_idx).second)
                return;
            auto &ref = files_[re_idx];
            for (auto sid : ref.public_syms) {
                auto &data   = ref.symbols.get(sid);
                auto sym_str = interner_.lookup(data.name);
                auto name    = arena_str(arena_, std::string(sym_str));
                if (file.is_from) {
                    declare_re_export(name, SymbolVisibility::Public, 0, data.kind, data.decl_id,
                                      data.span, data.target, data.doc_span, re_idx, sid);
                    auto qualified =
                        arena_str(arena_, std::string(ls) + "." + std::string(sym_str));
                    declare_re_export(qualified, SymbolVisibility::Public, 0, data.kind,
                                      data.decl_id, data.span, data.target, data.doc_span, re_idx,
                                      sid);
                } else {
                    auto qualified = arena_str(arena_, prefix + std::string(sym_str));
                    declare_re_export(qualified, SymbolVisibility::Public, 0, data.kind,
                                      data.decl_id, data.span, data.target, data.doc_span, re_idx,
                                      sid);
                }
            }
            for (auto sid : ref.module_syms) {
                auto &data = ref.symbols.get(sid);
                if (data.mod_depth >= 0 && call_depth != data.mod_depth)
                    continue;
                auto sym_str = interner_.lookup(data.name);
                auto name    = arena_str(arena_, std::string(sym_str));
                if (file.is_from) {
                    declare_re_export(name, SymbolVisibility::Module, data.mod_depth, data.kind,
                                      data.decl_id, data.span, data.target, data.doc_span, re_idx,
                                      sid);
                    auto qualified =
                        arena_str(arena_, std::string(ls) + "." + std::string(sym_str));
                    declare_re_export(qualified, SymbolVisibility::Module, data.mod_depth,
                                      data.kind, data.decl_id, data.span, data.target,
                                      data.doc_span, re_idx, sid);
                } else {
                    auto qualified = arena_str(arena_, prefix + std::string(sym_str));
                    declare_re_export(qualified, SymbolVisibility::Module, data.mod_depth,
                                      data.kind, data.decl_id, data.span, data.target,
                                      data.doc_span, re_idx, sid);
                }
            }
            for (auto r : ref.re_exported_files)
                self(self, r);
        };

        for (auto re_idx : file.re_exported_files)
            process_re_exports(process_re_exports, re_idx);
    }
}

memory::Optional<ImportManager::SymOrigin> ImportManager::originOf(SymId main_sym) const {
    if (main_sym >= sym_origins_.size())
        return {};
    auto &o = sym_origins_[main_sym];
    if (o.file_idx == size_t(-1))
        return {};
    return o;
}

void ImportManager::setVisibleRoots(std::vector<std::string> roots) {
    visible_roots_ = std::move(roots);
}

void ImportManager::setAssetRoots(std::vector<std::string> roots) {
    asset_roots_ = std::move(roots);
}

} // namespace zith::symbols
