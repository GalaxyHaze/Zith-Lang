// impl/import/symbol-resolver.hpp — v2 Symbol Resolution (current implementation)
//
// Provides fully qualified path resolution, duplicate detection,
// function overloading validation, and alias support.
// This is the v2 implementation: all new consumers should use this
// instead of v1 (symbol-table.hpp).
#pragma once

#include "module-registry.hpp"
#include "symbol-table.hpp"
#include <unordered_map>

#include <cstdio>
#include <vector>
#include <string>
#include <optional>

namespace zith {
namespace import {

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    ModuleNotFound = 1,
    SymbolNotFound = 2,
    DuplicateSymbol = 3,
    InvalidOverload = 4,
    AliasConflict = 5,
    CircularDependency = 6,
    SymbolNotExported = 7,
    AmbiguousResolution = 8,
};

// ============================================================================
// Error
// ============================================================================

struct Error {
    ErrorCode code;
    std::string message;
    std::string file_path;
    uint32_t line;
    uint32_t column;
    std::vector<std::string> details;

    Error(ErrorCode c, std::string msg)
        : code(c), message(std::move(msg)), line(0), column(0) {}

    Error(ErrorCode c, std::string msg, std::string path, uint32_t l, uint32_t col = 0)
        : code(c), message(std::move(msg)), file_path(std::move(path)), line(l), column(col) {}

    std::string to_string() const;
};

// ============================================================================
// SymbolEntry Resolution Result
// ============================================================================

class ResolvedSymbol {
public:
    ResolvedSymbol() : symbol_(nullptr), module_name_(), is_ambiguous_(false) {}
    ResolvedSymbol(SymbolEntry* sym, const std::string& module_name)
        : symbol_(sym), module_name_(module_name), is_ambiguous_(false) {}

    explicit operator bool() const { return symbol_ != nullptr; }

    SymbolEntry* symbol() const { return symbol_; }
    const std::string& module_name() const { return module_name_; }
    bool is_ambiguous() const { return is_ambiguous_; }

    void set_ambiguous() { is_ambiguous_ = true; }
    void add_candidate(SymbolEntry* sym) {
        candidates_.push_back(sym);
        is_ambiguous_ = true;
    }

    const std::vector<SymbolEntry*>& candidates() const { return candidates_; }

private:
    SymbolEntry* symbol_;
    std::string module_name_;
    bool is_ambiguous_;
    std::vector<SymbolEntry*> candidates_;
};

// ============================================================================
// Alias
// ============================================================================

struct Alias {
    std::string alias_name;
    std::string original_path;
    std::string target_module;
    std::string target_symbol;
    SourceLocation location;

    Alias(std::string alias, std::string original, SourceLocation loc)
        : alias_name(std::move(alias))
        , original_path(std::move(original))
        , location(loc) {}
};

// ============================================================================
// SymbolEntry Resolver
// ============================================================================

class SymbolResolver : public Singleton<SymbolResolver> {
public:

private:
    friend struct Singleton<SymbolResolver>;
    SymbolResolver() = default;

public:
    // Resolution
    ResolvedSymbol resolve(const std::string& fully_qualified_path);
    ResolvedSymbol resolve_in_module(const std::string& module_name,
                                      const std::string& symbol_name);

    // Module-level resolution (for import std.io;)
    ResolvedSymbol resolve_module(const std::string& module_name);

    // Multiple resolution (for auto-discovery)
    std::vector<ResolvedSymbol> resolve_all(const std::string& symbol_name);

// Alias management
    bool add_alias(const std::string& alias_name,
                  const std::string& target_path,
                  SourceLocation loc);

    std::optional<SymbolEntry> resolve_alias(const std::string& alias_name) const;

    bool alias_exists(const std::string& alias_name) const {
        return aliases_.contains(alias_name);
    }

    bool remove_alias(const std::string& alias_name);

    std::vector<std::string> list_aliases() const;

    // Validation
    std::vector<Error> validate_symbol(const std::string& module_name,
                                         const SymbolEntry& symbol);

    std::vector<Error> validate_module(Module& mod);

    // Conflict detection
    std::optional<Error> detect_duplicate(const std::string& module_name,
                                            const SymbolEntry& new_symbol);

    // Overload validation
    bool is_valid_overload(const std::string& module_name,
                           const SymbolEntry& new_function);

    // Reset
    void clear();

    // Error collection
    const std::vector<Error>& errors() const;
    void clear_errors();

private:
    // Parse fully qualified name into components
    std::vector<std::string> parse_path(const std::string& path) const;

    // Find module and symbol from path components
    ResolvedSymbol resolve_path(const std::vector<std::string>& components);

    // Check if two symbols are valid overloads
    bool are_valid_overloads(const SymbolEntry& a, const SymbolEntry& b) const;

    using AliasMap = std::unordered_map<std::string, Alias>;
    AliasMap aliases_;
    std::vector<Error> errors_;
};

inline std::string Error::to_string() const {
    std::string result = "Error " + std::to_string(static_cast<int>(code)) + ": " + message;
    if (!file_path.empty()) {
        result += " (" + file_path + ":" + std::to_string(line);
        if (column > 0) {
            result += ":" + std::to_string(column);
        }
        result += ")";
    }
    for (const auto& detail : details) {
        result += "\n  → " + detail;
    }
    return result;
}

inline std::vector<std::string> SymbolResolver::parse_path(const std::string& path) const {
    std::vector<std::string> components;
    std::string current;

    for (char c : path) {
        if (c == '.' || c == '/') {
            if (!current.empty()) {
                components.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        components.push_back(current);
    }

    return components;
}

inline ResolvedSymbol SymbolResolver::resolve(const std::string& fully_qualified_path) {
    if (auto it = aliases_.find(fully_qualified_path); it != aliases_.end()) {
        const Alias& alias = it->second;
        return resolve(alias.original_path);
    }

    auto components = parse_path(fully_qualified_path);
    if (components.empty()) {
        return ResolvedSymbol();
    }

    return resolve_path(components);
}

inline ResolvedSymbol SymbolResolver::resolve_path(const std::vector<std::string>& components) {
    if (components.empty()) {
        return ResolvedSymbol();
    }

    // If there's only one component, search all modules
    if (components.size() == 1) {
        const std::string& name = components[0];
        auto result = resolve_all(name);
        if (result.size() == 1) {
            return result[0];
        }
        if (result.size() > 1) {
            auto res = result[0];
            for (auto& r : result) {
                if (r.symbol()) {
                    r.set_ambiguous();
                    res.add_candidate(r.symbol());
                }
            }
            return res;
        }
        return ResolvedSymbol();
    }

    // Multiple components: find module by trying progressively longer names
    std::string module_name = components[0];
    auto mod = ModuleRegistry::instance().get_module(module_name);
    size_t module_parts = 1;
    
    if (!mod) {
        // Try to find the correct module by joining components
        for (size_t i = 1; i < components.size(); ++i) {
            std::string try_mod = components[0];
            for (size_t j = 1; j <= i; ++j) {
                try_mod += "." + components[j];
            }
            if (ModuleRegistry::instance().exists(try_mod)) {
                module_name = try_mod;
                mod = ModuleRegistry::instance().get_module(module_name);
                module_parts = i + 1;
                break;
            }
        }
    }
    
    if (!mod) {
        errors_.emplace_back(ErrorCode::ModuleNotFound,
                          "Module not found for path");
        return ResolvedSymbol();
    }

    // Build symbol name from remaining components after the module
    std::string symbol_name;
    for (size_t i = module_parts; i < components.size(); ++i) {
        if (!symbol_name.empty()) symbol_name += ".";
        symbol_name += components[i];
    }
    return resolve_in_module(module_name, symbol_name);
}

inline ResolvedSymbol SymbolResolver::resolve_in_module(const std::string& module_name,
                                                    const std::string& symbol_name) {
    auto mod = ModuleRegistry::instance().get_module(module_name);
    if (!mod) {
        errors_.emplace_back(ErrorCode::ModuleNotFound,
                          "Module '" + module_name + "' not found");
        return ResolvedSymbol();
    }

    auto* sym = mod->find_symbol(symbol_name);
    if (!sym) {
        errors_.emplace_back(ErrorCode::SymbolNotFound,
                          "SymbolEntry '" + symbol_name + "' not found in module '" + module_name + "'");
        return ResolvedSymbol();
    }

    if (!sym->is_exported() && sym->visibility() != Visibility::Public) {
        errors_.emplace_back(ErrorCode::SymbolNotExported,
                          "SymbolEntry '" + symbol_name + "' is not exported from module '" + module_name + "'");
        return ResolvedSymbol();
    }

    return ResolvedSymbol(sym, module_name);
}

inline ResolvedSymbol SymbolResolver::resolve_module(const std::string& module_name) {
    auto mod = ModuleRegistry::instance().get_module(module_name);
    if (!mod) {
        errors_.emplace_back(ErrorCode::ModuleNotFound,
                          "Module '" + module_name + "' not found");
        return ResolvedSymbol();
    }
    return ResolvedSymbol(nullptr, module_name);
}

inline std::vector<ResolvedSymbol> SymbolResolver::resolve_all(const std::string& symbol_name) {
    std::vector<ResolvedSymbol> results;

    for (const auto& name : ModuleRegistry::instance().list_modules()) {
        auto mod = ModuleRegistry::instance().get_module(name);
        if (mod) {
            auto* sym = mod->find_symbol(symbol_name);
            if (sym && sym->is_exported()) {
                results.emplace_back(sym, name);
            }
        }
    }

    return results;
}

inline bool SymbolResolver::add_alias(const std::string& alias_name,
                                        const std::string& target_path,
                                        SourceLocation loc) {
    if (aliases_.contains(alias_name)) {
        errors_.emplace_back(ErrorCode::AliasConflict,
                           "Alias '" + alias_name + "' already exists",
                           loc.file_path, loc.line, loc.column);
        return false;
    }

// Validate target exists
    auto result = resolve(target_path);
    if (!result) {
        errors_.emplace_back(ErrorCode::SymbolNotFound,
                           "Target symbol '" + target_path + "' for alias '" + alias_name + "' not found",
                           loc.file_path, loc.line, loc.column);
        return false;
    }

    // Parse target to get module and symbol
    auto components = parse_path(target_path);
    if (components.empty()) {
        return false;
    }

    Alias alias(alias_name, target_path, loc);
    if (components.size() >= 1) {
        alias.target_module = components[0];
        // Find correct module name
        for (size_t i = 1; i < components.size(); ++i) {
            std::string try_mod = components[0];
            for (size_t j = 1; j <= i; ++j) {
                try_mod += "." + components[j];
            }
            if (ModuleRegistry::instance().exists(try_mod)) {
                alias.target_module = try_mod;
                break;
            }
        }
    }
    if (components.size() >= 2) {
        // Find symbol name after module
        size_t module_parts = 1;
        std::string test_mod = components[0];
        for (size_t i = 1; i < components.size(); ++i) {
            test_mod += "." + components[i];
            if (ModuleRegistry::instance().exists(test_mod)) {
                module_parts = i + 1;
            }
        }
        for (size_t i = module_parts; i < components.size(); ++i) {
            if (!alias.target_symbol.empty()) alias.target_symbol += ".";
            alias.target_symbol += components[i];
        }
    }
    aliases_.emplace(alias_name, std::move(alias));
    return true;
}

inline std::optional<SymbolEntry> SymbolResolver::resolve_alias(const std::string& alias_name) const {
    auto it = aliases_.find(alias_name);
    if (it == aliases_.end()) {
        return std::nullopt;
    }

    const Alias& alias = it->second;
    
    auto mod = ModuleRegistry::instance().get_module(alias.target_module);
    if (!mod) {
        return std::nullopt;
    }

    auto* sym = mod->find_symbol(alias.target_symbol);
    if (!sym) {
        return std::nullopt;
    }

    return *sym;
}

inline void SymbolResolver::clear() {
    aliases_.clear();
    clear_errors();
}

inline const std::vector<Error>& SymbolResolver::errors() const {
    return errors_;
}

inline void SymbolResolver::clear_errors() {
    errors_.clear();
}

inline bool SymbolResolver::remove_alias(const std::string& alias_name) {
    return aliases_.erase(alias_name) > 0;
}

inline std::vector<std::string> SymbolResolver::list_aliases() const {
    std::vector<std::string> names;
    names.reserve(aliases_.size());
    for (const auto& kv : aliases_) {
        names.push_back(kv.first);
    }
    return names;
}

inline std::vector<Error> SymbolResolver::validate_symbol(const std::string& module_name,
                                                           const SymbolEntry& symbol) {
    std::vector<Error> validation_errors;

    auto mod = ModuleRegistry::instance().get_module(module_name);
    if (!mod) {
        validation_errors.emplace_back(ErrorCode::ModuleNotFound,
                                      "Module '" + module_name + "' not found");
        return validation_errors;
    }

    // Check for duplicate
    if (auto dup = detect_duplicate(module_name, symbol)) {
        validation_errors.push_back(*dup);
    }

    // If it's a function, check overload validity
    if (symbol.kind() == SymbolKind::Function && symbol.signature().has_value()) {
        if (!is_valid_overload(module_name, symbol)) {
            validation_errors.emplace_back(ErrorCode::InvalidOverload,
                                           "Function '" + symbol.name() +
                                           "' has invalid overload (duplicate signature)");
        }
    }

    return validation_errors;
}

inline std::vector<Error> SymbolResolver::validate_module(Module& /*mod*/) {
    std::vector<Error> validation_errors;
    return validation_errors;
}

inline std::optional<Error> SymbolResolver::detect_duplicate(const std::string& module_name,
                                                               const SymbolEntry& new_symbol) {
    auto mod = ModuleRegistry::instance().get_module(module_name);
    if (!mod) {
        return std::nullopt;
    }

    auto* existing = mod->find_symbol(new_symbol.name());
    if (existing) {
        if (new_symbol.has_identical_signature(*existing)) {
            return Error(ErrorCode::DuplicateSymbol,
                        "Duplicate symbol '" + new_symbol.name() + "' in module '" + module_name + "'",
                        new_symbol.location().file_path,
                        new_symbol.location().line,
                        new_symbol.location().column);
        }
    }

    return std::nullopt;
}

inline bool SymbolResolver::is_valid_overload(const std::string& module_name,
                                            const SymbolEntry& new_function) {
    auto mod = ModuleRegistry::instance().get_module(module_name);
    if (!mod) {
        return true;
    }

    const auto& symbols = mod->symbols();
    for (const auto& sym : symbols) {
        if (sym.name() == new_function.name() && sym.kind() == SymbolKind::Function) {
            // Check if signatures are EXACTLY the same (not valid overload)
            bool identical = false;
            if (new_function.signature().has_value() && sym.signature().has_value()) {
                identical = (*new_function.signature() == *sym.signature());
            }
            if (identical) {
                return false;  // Same signature = invalid overload
            }
        }
    }

    return true;
}

inline bool SymbolResolver::are_valid_overloads(const SymbolEntry& a, const SymbolEntry& b) const {
    if (!a.signature().has_value() || !b.signature().has_value()) {
        return a.name() != b.name();
    }

    return a.has_compatible_signature(b);
}

// ============================================================================
// Convenience functions
// ============================================================================

inline ResolvedSymbol resolve_symbol(const std::string& fully_qualified_path) {
    return SymbolResolver::instance().resolve(fully_qualified_path);
}

inline bool symbol_exists(const std::string& fully_qualified_path) {
    auto result = resolve_symbol(fully_qualified_path);
    return static_cast<bool>(result);
}

// ============================================================================
// Top-level reset
// ============================================================================

inline void reset_all() {
    ModuleRegistry::instance().clear();
    SymbolTable::instance().clear();
    SymbolResolver::instance().clear();
}

} // namespace import
} // namespace zith