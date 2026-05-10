// impl/import/symbol_table.hpp — Global symbol table for Zith
//
// Provides centralized symbol resolution.
// Thread-unsafe (single-threaded compiler context).
#pragma once

#include "import.hpp"
#include <unordered_map>

#include <vector>
#include <memory>
#include <string>
#include <optional>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}

namespace zith {
namespace import {

// ============================================================================
// Symbol Table Entry
// ============================================================================

struct SymbolTableEntry {
    Import* import_;
    Visibility visibility_;
    uint32_t index_;

    SymbolTableEntry(Import* imp = nullptr, Visibility vis = Visibility::Public, uint32_t idx = 0)
        : import_(imp), visibility_(vis), index_(idx) {}
};

// ============================================================================
// Symbol Resolution Result
// ============================================================================

class SymbolResolution {
public:
    SymbolResolution() : entry_(nullptr), index_(0) {}
    SymbolResolution(SymbolTableEntry* entry, uint32_t idx)
        : entry_(entry), index_(idx) {}

    explicit operator bool() const { return entry_ != nullptr; }

    Import* import() const { return entry_ ? entry_->import_ : nullptr; }
    uint32_t index() const { return index_; }
    std::optional<Symbol> get_symbol() const;

private:
    SymbolTableEntry* entry_;
    uint32_t index_;
};

// ============================================================================
// Global Symbol Table (Singleton)
// ============================================================================

class SymbolTable {
public:
    static SymbolTable& instance() {
        static SymbolTable inst;
        return inst;
    }

    bool register_import(Import& imp) {
        std::string name = imp.name();
        if (name.empty()) {
            return false;
        }

        if (imports_.contains(name)) {
            return false;
        }

        SymbolTableEntry entry(&imp);
        imports_.emplace(name, entry);
        index_import(imp);
        return true;
    }

    void unregister_import(const std::string& name) {
        auto it = imports_.find(name);
        if (it != imports_.end()) {
            unindex_import(*it->second.import_);
            imports_.erase(it);
        }
    }

    auto resolve(const std::string& fully_qualified_name) {
        size_t sep_pos = fully_qualified_name.find('/');
        if (sep_pos != std::string::npos) {
            size_t candidate_pos = sep_pos;
            while (candidate_pos != std::string::npos) {
                std::string import_name = fully_qualified_name.substr(0, candidate_pos);
                auto it = imports_.find(import_name);
                if (it != imports_.end()) {
                    Import* imp = it->second.import_;
                    std::string symbol_name = fully_qualified_name.substr(candidate_pos + 1);
                    return resolve_in_import(*imp, symbol_name);
                }
                candidate_pos = fully_qualified_name.find('/', candidate_pos + 1);
            }
            return SymbolResolution();
        }

        if (imports_.contains(fully_qualified_name)) {
            return resolve_local(fully_qualified_name);
        }
        auto dot_pos = fully_qualified_name.find('.');
        if (dot_pos == std::string::npos) {
            return resolve_local(fully_qualified_name);
        }
        std::string import_name = fully_qualified_name.substr(0, dot_pos);
        if (!imports_.contains(import_name)) {
            return resolve_local(fully_qualified_name);
        }
        Import* imp = imports_[import_name].import_;
        return resolve_in_import(*imp, fully_qualified_name.substr(dot_pos + 1));
    }

    SymbolResolution resolve_local(const std::string& name) {
        for (auto& kv : imports_) {
            auto result = resolve_in_import(*kv.second.import_, name);
            if (result) {
                return result;
            }
        }
        return SymbolResolution();
    }

    std::vector<SymbolResolution> resolve_all(const std::string& fully_qualified_name) {
        std::vector<SymbolResolution> results;

        size_t sep_pos = fully_qualified_name.find('/');
        if (sep_pos != std::string::npos) {
            size_t candidate_pos = sep_pos;
            while (candidate_pos != std::string::npos) {
                std::string import_name = fully_qualified_name.substr(0, candidate_pos);
                auto it = imports_.find(import_name);
                if (it != imports_.end()) {
                    Import* imp = it->second.import_;
                    auto result = resolve_in_import(*imp, fully_qualified_name.substr(candidate_pos + 1));
                    if (result) results.push_back(result);
                    return results;
                }
                candidate_pos = fully_qualified_name.find('/', candidate_pos + 1);
            }
            return results;
        }

        if (imports_.contains(fully_qualified_name)) {
            Import* imp = imports_[fully_qualified_name].import_;
            auto result = resolve_in_import(*imp, fully_qualified_name);
            if (result) results.push_back(result);
            return results;
        }
        auto dot_pos = fully_qualified_name.find('.');
        if (dot_pos == std::string::npos) {
            for (auto& kv : imports_) {
                auto result = resolve_in_import(*kv.second.import_, fully_qualified_name);
                if (result) results.push_back(result);
            }
            return results;
        }
        std::string import_name = fully_qualified_name.substr(0, dot_pos);
        if (imports_.contains(import_name)) {
            Import* imp = imports_[import_name].import_;
            auto result = resolve_in_import(*imp, fully_qualified_name.substr(dot_pos + 1));
            if (result) results.push_back(result);
        }
        return results;
    }

    bool is_registered(const std::string& name) const {
        return imports_.contains(name);
    }

    std::vector<std::string> list_imports() const {
        std::vector<std::string> names;
        names.reserve(imports_.size());
        for (const auto& kv : imports_) {
            names.push_back(kv.first);
        }
        return names;
    }

    Import* get_import(const std::string& name) const {
        auto it = imports_.find(name);
        return it != imports_.end() ? it->second.import_ : nullptr;
    }

    void clear() {
        imports_.clear();
        symbols_by_name_.clear();
    }

private:
    SymbolTable() = default;

    void index_import(Import& imp) {
        auto index_arr = [this, &imp](const std::vector<Symbol>& arr, Visibility vis) {
            for (uint32_t i = 0; i < arr.size(); ++i) {
                const Symbol& sym = arr[i];
                std::string fully_qualified = imp.name() + "." + sym.name();
                symbols_by_name_[fully_qualified] = SymbolTableEntry(&imp, vis, i);
            }
        };

        index_arr(imp.public_types(), Visibility::Public);
        index_arr(imp.public_functions(), Visibility::Public);
        index_arr(imp.public_traits(), Visibility::Public);
        index_arr(imp.protected_types(), Visibility::Protected);
        index_arr(imp.protected_functions(), Visibility::Protected);
        index_arr(imp.protected_traits(), Visibility::Protected);
        index_arr(imp.private_types(), Visibility::Private);
        index_arr(imp.private_functions(), Visibility::Private);
        index_arr(imp.private_traits(), Visibility::Private);
    }

    void unindex_import(Import& imp) {
        (void)imp;
    }

    SymbolResolution resolve_in_import(Import& imp, const std::string& symbol_name) {
        auto it = imports_.find(imp.name());
        if (it == imports_.end()) {
            return SymbolResolution();
        }
        SymbolTableEntry* entry_ptr = &it->second;

        auto search_arr = [&symbol_name, entry_ptr](const std::vector<Symbol>& arr, uint32_t base) -> SymbolResolution {
            for (uint32_t i = 0; i < arr.size(); ++i) {
                if (arr[i].name() == symbol_name) {
                    return SymbolResolution(entry_ptr, base + i);
                }
            }
            return SymbolResolution();
        };
        uint32_t base = 0;

        auto result = search_arr(imp.public_types(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.public_types().size());

        result = search_arr(imp.public_functions(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.public_functions().size());

        result = search_arr(imp.public_traits(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.public_traits().size());

        result = search_arr(imp.protected_types(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.protected_types().size());

        result = search_arr(imp.protected_functions(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.protected_functions().size());

        result = search_arr(imp.protected_traits(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.protected_traits().size());

        result = search_arr(imp.private_types(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.private_types().size());

        result = search_arr(imp.private_functions(), base);
        if (result) return result;
        base += static_cast<uint32_t>(imp.private_functions().size());

        result = search_arr(imp.private_traits(), base);
        if (result) return result;

        return SymbolResolution();
    }

    using ImportMap = std::unordered_map<std::string, SymbolTableEntry>;
    using SymbolMap = std::unordered_map<std::string, SymbolTableEntry>;

    ImportMap imports_;
    SymbolMap symbols_by_name_;
};

// ============================================================================
// Symbol Implementation
// ============================================================================

inline std::optional<Symbol> SymbolResolution::get_symbol() const {
    if (!entry_) return std::nullopt;

    Import* imp = entry_->import_;
    uint32_t idx = index_;

    auto& arr = imp->public_types();
    if (idx < arr.size()) return arr[idx];
    idx -= arr.size();

    auto& funcs = imp->public_functions();
    if (idx < funcs.size()) return funcs[idx];
    idx -= funcs.size();

    auto& traits = imp->public_traits();
    if (idx < traits.size()) return traits[idx];
    idx -= traits.size();

    auto& prot_types = imp->protected_types();
    if (idx < prot_types.size()) return prot_types[idx];
    idx -= prot_types.size();

    auto& prot_funcs = imp->protected_functions();
    if (idx < prot_funcs.size()) return prot_funcs[idx];
    idx -= prot_funcs.size();

    auto& prot_traits = imp->protected_traits();
    if (idx < prot_traits.size()) return prot_traits[idx];
    idx -= prot_traits.size();

    auto& priv_types = imp->private_types();
    if (idx < priv_types.size()) return priv_types[idx];
    idx -= priv_types.size();

    auto& priv_funcs = imp->private_functions();
    if (idx < priv_funcs.size()) return priv_funcs[idx];
    idx -= priv_funcs.size();

    auto& priv_traits = imp->private_traits();
    if (idx < priv_traits.size()) return priv_traits[idx];

    return std::nullopt;
}

// ============================================================================
// Convenience functions
// ============================================================================

inline Import* resolve_import(const std::string& name) {
    return SymbolTable::instance().get_import(name);
}

inline std::optional<Symbol> resolve_symbol(const std::string& fully_qualified_name) {
    auto result = SymbolTable::instance().resolve(fully_qualified_name);
    return result.get_symbol();
}

} // namespace import
} // namespace zith
#endif