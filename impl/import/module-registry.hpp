// impl/import/module-registry.hpp — Module registry for Zith's import system
//
// Provides centralized module management with file-to-module mapping,
// tracking each module's source file, exported symbols, and version.
#pragma once

#include "import.hpp"
#include <unordered_map>

#include <vector>
#include <string>
#include <optional>
#include <filesystem>
#include <memory>

namespace zith::import {

// ============================================================================
// Source Location
// ============================================================================

struct SourceLocation {
    std::string file_path;
    uint32_t line;
    uint32_t column;

    SourceLocation() : line(0), column(0) {}
    SourceLocation(std::string path, uint32_t l, uint32_t c = 0)
        : file_path(std::move(path)), line(l), column(c) {}
};

// ============================================================================
// Type Signature (for function overloading)
// ============================================================================

struct TypeSignature {
    std::vector<std::string> param_types;
    std::string return_type;

    bool operator==(const TypeSignature& other) const {
        return param_types == other.param_types && return_type == other.return_type;
    }

    bool operator!=(const TypeSignature& other) const {
        return !(*this == other);
    }

    std::string to_string() const;

    bool is_compatible_with(const TypeSignature& other) const;
};

inline std::string TypeSignature::to_string() const {
    std::string result = "(";
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0) result += ", ";
        result += param_types[i];
    }
    result += ") -> " + return_type;
    return result;
}

inline bool TypeSignature::is_compatible_with(const TypeSignature& other) const {
    if (param_types.size() != other.param_types.size()) {
        return false;
    }

    for (size_t i = 0; i < param_types.size(); ++i) {
        if (param_types[i] != other.param_types[i]) {
            return false;
        }
    }

    return return_type == other.return_type;
}

// ============================================================================
// Symbol Entry (extended with location and signature)
// ============================================================================
// ============================================================================

class SymbolEntry {
public:
    SymbolEntry() = default;

    SymbolEntry(std::string name, SymbolKind kind, Visibility vis,
               SourceLocation loc = SourceLocation{})
        : name_(std::move(name))
        , kind_(kind)
        , visibility_(vis)
        , location_(loc)
        , is_exported_(vis == Visibility::Public) {}

    const std::string& name() const { return name_; }
    SymbolKind kind() const { return kind_; }
    Visibility visibility() const { return visibility_; }
    const SourceLocation& location() const { return location_; }
    bool is_exported() const { return is_exported_; }

    void set_location(SourceLocation loc) { location_ = std::move(loc); }
    void set_exported(bool exported) { is_exported_ = exported; }
    void set_signature(TypeSignature sig) { signature_ = std::move(sig); }
    const std::optional<TypeSignature>& signature() const { return signature_; }

    const std::string& fully_qualified() const { return fully_qualified_; }
    void set_fully_qualified(std::string fq) { fully_qualified_ = std::move(fq); }

    bool matches_name(const std::string& n) const { return name_ == n; }

    bool has_identical_signature(const SymbolEntry& other) const {
        if (!signature_.has_value() || !other.signature_.has_value()) {
            return false;
        }
        return *signature_ == *other.signature_;
    }

    bool has_compatible_signature(const SymbolEntry& other) const {
        if (!signature_.has_value() || !other.signature_.has_value()) {
            return false;
        }
        return signature_->is_compatible_with(*other.signature_);
    }

private:
    std::string name_;
    std::string fully_qualified_;
    SymbolKind kind_;
    Visibility visibility_;
    SourceLocation location_;
    bool is_exported_;
    std::optional<TypeSignature> signature_;
};

// ============================================================================
// Module
// ============================================================================

class Module {
public:
    Module() = default;

    Module(std::string name, std::filesystem::path source_file, uint32_t version = 0)
        : name_(std::move(name))
        , source_file_(std::move(source_file))
        , version_(version) {}

    const std::string& name() const { return name_; }
    const std::filesystem::path& source_file() const { return source_file_; }
    uint32_t version() const { return version_; }

    void set_version(uint32_t v) { version_ = v; }

    // Symbol management
    void add_symbol(SymbolEntry sym) {
        auto name = sym.name();
        sym.set_fully_qualified(name_ + "." + name);

        auto it = symbol_index_.find(name);
        if (it == symbol_index_.end()) {
            symbols_.push_back(std::move(sym));
            symbol_index_[name] = static_cast<uint32_t>(symbols_.size()) - 1;
        } else {
            symbols_[it->second] = std::move(sym);
        }
    }

    const std::vector<SymbolEntry>& symbols() const { return symbols_; }

    SymbolEntry* find_symbol(const std::string& name) {
        auto it = symbol_index_.find(name);
        if (it != symbol_index_.end()) {
            return &symbols_[it->second];
        }
        return nullptr;
    }

    const SymbolEntry* find_symbol(const std::string& name) const {
        auto it = symbol_index_.find(name);
        if (it != symbol_index_.end()) {
            return &symbols_[it->second];
        }
        return nullptr;
    }

    std::vector<SymbolEntry*> find_symbols_by_kind(SymbolKind kind) {
        std::vector<SymbolEntry*> results;
        for (auto& sym : symbols_) {
            if (sym.kind() == kind) {
                results.push_back(&sym);
            }
        }
        return results;
    }

    bool has_symbol(const std::string& name) const {
        return symbol_index_.contains(name);
    }

    size_t symbol_count() const { return symbols_.size(); }

private:
    std::string name_;
    std::filesystem::path source_file_;
    uint32_t version_;

    std::vector<SymbolEntry> symbols_;
    std::unordered_map<std::string, uint32_t> symbol_index_;
};

// ============================================================================
// Module Registry (Singleton)
// ============================================================================

class ModuleRegistry : public Singleton<ModuleRegistry> {
public:
    std::shared_ptr<Module> get_module(const std::string& name) {
        auto it = modules_.find(name);
        return it != modules_.end() ? it->second : nullptr;
    }

    bool exists(const std::string& name) const {
        return modules_.contains(name);
    }

    bool register_module(Module mod) {
        std::string mod_name = mod.name();
        if (modules_.contains(mod_name)) {
            return false;
        }
        modules_[std::move(mod_name)] = std::make_shared<Module>(std::move(mod));
        return true;
    }

    void clear() {
        modules_.clear();
    }

    std::vector<std::string> list_modules() const {
        std::vector<std::string> names;
        names.reserve(modules_.size());
        for (const auto& [name, _] : modules_) {
            names.push_back(name);
        }
        return names;
    }

private:
    friend struct Singleton<ModuleRegistry>;
    ModuleRegistry() = default;

    using ModuleMap = std::unordered_map<std::string, std::shared_ptr<Module>>;
    ModuleMap modules_;
};

// ============================================================================
// Convenience functions
// ============================================================================

inline std::shared_ptr<Module> get_module(const std::string& name) {
    return ModuleRegistry::instance().get_module(name);
}

inline bool module_exists(const std::string& name) {
    return ModuleRegistry::instance().exists(name);
}

} // namespace zith