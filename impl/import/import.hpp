// impl/import/import.hpp — C++ wrapper for Zith's import system
//
// Provides RAII wrappers, convenience methods, and STL-compatible
// interfaces for the import system.
#pragma once

#include "zith/import.h"

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <cstring>
#include <unordered_map>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}

namespace zith {
namespace import {

// ============================================================================
// Enums (C++ wrappers)
// ============================================================================

enum class SymbolKind : uint8_t {
    Struct    = ZITH_SYM_STRUCT,
    Union     = ZITH_SYM_UNION,
    Component = ZITH_SYM_COMPONENT,
    Entity    = ZITH_SYM_ENTITY,
    Enum      = ZITH_SYM_ENUM,
    Function  = ZITH_SYM_FUNCTION,
    Trait     = ZITH_SYM_TRAIT,
    Family    = ZITH_SYM_FAMILY,
};

enum class Visibility : uint8_t {
    Private   = ZITH_IMPORT_VIS_PRIVATE,
    Public    = ZITH_IMPORT_VIS_PUBLIC,
    Protected = ZITH_IMPORT_VIS_PROTECTED,
};

enum class ChangeKind : uint8_t {
    Add    = 0,
    Remove = 1,
    Modify = 2,
};

// ============================================================================
// Span-like helpers
// ============================================================================

template <typename T>
class Span {
public:
    Span() : data_(nullptr), length_(0) {}
    Span(T* data, uint32_t length) : data_(data), length_(length) {}

    T* data() const { return data_; }
    uint32_t length() const { return length_; }

    T& operator[](uint32_t i) const { return data_[i]; }

    auto begin() const { return data_; }
    auto end() const { return data_ + length_; }

private:
    T* data_;
    uint32_t length_;
};

using SymbolSpan = Span<ZithSymbol>;
using ChangeSpan = Span<ZithChange>;

// ============================================================================
// Symbol (C++ wrapper)
// ============================================================================

class Symbol {
public:
    Symbol() : name_(""), kind_(SymbolKind::Struct), visibility_(Visibility::Public), decl_(nullptr) {}
    Symbol(std::string name, SymbolKind kind, Visibility vis, void* decl = nullptr)
        : name_(std::move(name)), kind_(kind), visibility_(vis), decl_(decl) {}

    const std::string& name() const { return name_; }
    SymbolKind kind() const { return kind_; }
    Visibility visibility() const { return visibility_; }
    void* decl() const { return decl_; }

    void set_decl(void* d) { decl_ = d; }

    ZithSymbol to_c() const {
        ZithSymbol s;
        s.name = const_cast<char*>(name_.c_str());
        s.kind = static_cast<ZithSymbolKind>(kind_);
        s.visibility = static_cast<ZithImportVisibility>(visibility_);
        s.decl = decl_;
        return s;
    }

    static Symbol from_c(ZithSymbol s) {
        return Symbol(s.name ? s.name : "", static_cast<SymbolKind>(s.kind),
                       static_cast<Visibility>(s.visibility), s.decl);
    }

private:
    std::string name_;
    SymbolKind kind_;
    Visibility visibility_;
    void* decl_;
};

// ============================================================================
// Change (C++ wrapper)
// ============================================================================

class Change {
public:
    Change() : kind_(ChangeKind::Add), symbol_name_(""), symbol_kind_(SymbolKind::Struct) {}
    Change(ChangeKind kind, std::string name, SymbolKind sym_kind)
        : kind_(kind), symbol_name_(std::move(name)), symbol_kind_(sym_kind) {}

    ChangeKind kind() const { return kind_; }
    const std::string& symbol_name() const { return symbol_name_; }
    SymbolKind symbol_kind() const { return symbol_kind_; }

    ZithChange to_c() const {
        ZithChange c;
        c.kind = static_cast<uint8_t>(kind_);
        c.symbol_name = const_cast<char*>(symbol_name_.c_str());
        c.symbol_kind = static_cast<ZithSymbolKind>(symbol_kind_);
        return c;
    }

    static Change from_c(ZithChange c) {
        return Change(static_cast<ChangeKind>(c.kind), c.symbol_name ? c.symbol_name : "",
                      static_cast<SymbolKind>(c.symbol_kind));
    }

private:
    ChangeKind kind_;
    std::string symbol_name_;
    SymbolKind symbol_kind_;
};

// ============================================================================
// Import (main C++ wrapper with RAII)
// ============================================================================

class Import {
public:
    Import() : Import("", 0) {}

    Import(std::string name, uint32_t version = 0)
        : name_(std::move(name)), version_(version), is_dirty_(true) {}

    ~Import() = default;

    Import(const Import&) = delete;
    Import& operator=(const Import&) = delete;

    Import(Import&& other) noexcept
        : name_(std::move(other.name_))
        , version_(other.version_)
        , is_dirty_(other.is_dirty_)
        , public_types_(std::move(other.public_types_))
        , public_functions_(std::move(other.public_functions_))
        , public_traits_(std::move(other.public_traits_))
        , protected_types_(std::move(other.protected_types_))
        , protected_functions_(std::move(other.protected_functions_))
        , protected_traits_(std::move(other.protected_traits_))
        , private_types_(std::move(other.private_types_))
        , private_functions_(std::move(other.private_functions_))
        , private_traits_(std::move(other.private_traits_))
        , changes_(std::move(other.changes_)) {
        other.is_dirty_ = false;
    }

    Import& operator=(Import&& other) noexcept {
        if (this != &other) {
            name_ = std::move(other.name_);
            version_ = other.version_;
            is_dirty_ = other.is_dirty_;
            public_types_ = std::move(other.public_types_);
            public_functions_ = std::move(other.public_functions_);
            public_traits_ = std::move(other.public_traits_);
            protected_types_ = std::move(other.protected_types_);
            protected_functions_ = std::move(other.protected_functions_);
            protected_traits_ = std::move(other.protected_traits_);
            private_types_ = std::move(other.private_types_);
            private_functions_ = std::move(other.private_functions_);
            private_traits_ = std::move(other.private_traits_);
            changes_ = std::move(other.changes_);
            other.is_dirty_ = false;
        }
        return *this;
    }

    // Accessors
    const std::string& name() const { return name_; }
    uint32_t version() const { return version_; }

    // Symbol addition
    void add_type(std::string name, SymbolKind kind, Visibility vis = Visibility::Public) {
        switch (vis) {
            case Visibility::Public: public_types_.push_back(Symbol{std::move(name), kind, vis, nullptr}); break;
            case Visibility::Protected: protected_types_.push_back(Symbol{std::move(name), kind, vis, nullptr}); break;
            case Visibility::Private: private_types_.push_back(Symbol{std::move(name), kind, vis, nullptr}); break;
        }
        mark_dirty();
    }

    void add_function(std::string name, Visibility vis = Visibility::Public) {
        switch (vis) {
            case Visibility::Public: public_functions_.push_back(Symbol{std::move(name), SymbolKind::Function, vis, nullptr}); break;
            case Visibility::Protected: protected_functions_.push_back(Symbol{std::move(name), SymbolKind::Function, vis, nullptr}); break;
            case Visibility::Private: private_functions_.push_back(Symbol{std::move(name), SymbolKind::Function, vis, nullptr}); break;
        }
        mark_dirty();
    }

    void add_trait(std::string name, Visibility vis = Visibility::Public) {
        switch (vis) {
            case Visibility::Public: public_traits_.push_back(Symbol{std::move(name), SymbolKind::Trait, vis, nullptr}); break;
            case Visibility::Protected: protected_traits_.push_back(Symbol{std::move(name), SymbolKind::Trait, vis, nullptr}); break;
            case Visibility::Private: private_traits_.push_back(Symbol{std::move(name), SymbolKind::Trait, vis, nullptr}); break;
        }
        mark_dirty();
    }

    void add_family(std::string name, Visibility vis = Visibility::Public) {
        switch (vis) {
            case Visibility::Public: public_traits_.push_back(Symbol{std::move(name), SymbolKind::Family, vis, nullptr}); break;
            case Visibility::Protected: protected_traits_.push_back(Symbol{std::move(name), SymbolKind::Family, vis, nullptr}); break;
            case Visibility::Private: private_traits_.push_back(Symbol{std::move(name), SymbolKind::Family, vis, nullptr}); break;
        }
        mark_dirty();
    }

    // Accessors by visibility
    std::vector<Symbol>& public_types() { return public_types_; }
    std::vector<Symbol>& protected_types() { return protected_types_; }
    std::vector<Symbol>& private_types() { return private_types_; }

    std::vector<Symbol>& public_functions() { return public_functions_; }
    std::vector<Symbol>& protected_functions() { return protected_functions_; }
    std::vector<Symbol>& private_functions() { return private_functions_; }

    std::vector<Symbol>& public_traits() { return public_traits_; }
    std::vector<Symbol>& protected_traits() { return protected_traits_; }
    std::vector<Symbol>& private_traits() { return private_traits_; }

    const std::vector<Symbol>& public_types() const { return public_types_; }
    const std::vector<Symbol>& protected_types() const { return protected_types_; }
    const std::vector<Symbol>& private_types() const { return private_types_; }

    const std::vector<Symbol>& public_functions() const { return public_functions_; }
    const std::vector<Symbol>& protected_functions() const { return protected_functions_; }
    const std::vector<Symbol>& private_functions() const { return private_functions_; }

    const std::vector<Symbol>& public_traits() const { return public_traits_; }
    const std::vector<Symbol>& protected_traits() const { return protected_traits_; }
    const std::vector<Symbol>& private_traits() const { return private_traits_; }

    // Dirty tracking
    bool is_dirty() const { return is_dirty_; }
    void mark_dirty() { is_dirty_ = true; }
    void mark_clean() { is_dirty_ = false; }

    // Change log
    void log_change(ChangeKind kind, std::string symbol_name, SymbolKind sym_kind) {
        changes_.push_back(Change(kind, std::move(symbol_name), sym_kind));
        mark_dirty();
    }

    const std::vector<Change>& changes() const { return changes_; }
    void clear_changes() {
        changes_.clear();
    }

    // Conversion to C ABI (deep copy, caller must free with zith_import_destroy)
    ZithImport* to_c() const {
        auto convert_arr = [](const std::vector<Symbol>& src) {
            ZithSymbolArray arr;
            arr.length = static_cast<uint32_t>(src.size());
            arr.capacity = static_cast<uint32_t>(src.capacity());
            arr.data = arr.length > 0 ? new ZithSymbol[arr.length] : nullptr;
            for (uint32_t i = 0; i < arr.length; ++i) {
                arr.data[i] = src[i].to_c();
            }
            return arr;
        };

        auto convert_changes = [](const std::vector<Change>& src) {
            ZithChangeArray arr;
            arr.length = static_cast<uint32_t>(src.size());
            arr.capacity = static_cast<uint32_t>(src.capacity());
            arr.data = arr.length > 0 ? new ZithChange[arr.length] : nullptr;
            for (uint32_t i = 0; i < arr.length; ++i) {
                arr.data[i] = src[i].to_c();
            }
            return arr;
        };

        ZithImport* imp = new ZithImport{};
        imp->name = new char[name_.size() + 1];
        std::strcpy(imp->name, name_.c_str());
        imp->version = version_;

        imp->public_types = convert_arr(public_types_);
        imp->public_functions = convert_arr(public_functions_);
        imp->public_traits = convert_arr(public_traits_);
        imp->protected_types = convert_arr(protected_types_);
        imp->protected_functions = convert_arr(protected_functions_);
        imp->protected_traits = convert_arr(protected_traits_);
        imp->private_types = convert_arr(private_types_);
        imp->private_functions = convert_arr(private_functions_);
        imp->private_traits = convert_arr(private_traits_);

        imp->is_dirty = is_dirty_ ? 1 : 0;
        imp->changes = convert_changes(changes_);

        return imp;
    }

    static Import from_c(const ZithImport* c_imp) {
        Import imp(c_imp->name ? c_imp->name : "", c_imp->version);

        auto copy_arr = [](const ZithSymbolArray& src, std::vector<Symbol>& dest) {
            for (uint32_t i = 0; i < src.length; ++i) {
                dest.push_back(Symbol::from_c(src.data[i]));
            }
        };

        copy_arr(c_imp->public_types, imp.public_types_);
        copy_arr(c_imp->public_functions, imp.public_functions_);
        copy_arr(c_imp->public_traits, imp.public_traits_);
        copy_arr(c_imp->protected_types, imp.protected_types_);
        copy_arr(c_imp->protected_functions, imp.protected_functions_);
        copy_arr(c_imp->protected_traits, imp.protected_traits_);
        copy_arr(c_imp->private_types, imp.private_types_);
        copy_arr(c_imp->private_functions, imp.private_functions_);
        copy_arr(c_imp->private_traits, imp.private_traits_);

        for (uint32_t i = 0; i < c_imp->changes.length; ++i) {
            imp.changes_.push_back(Change::from_c(c_imp->changes.data[i]));
        }

        imp.is_dirty_ = c_imp->is_dirty != 0;
        return imp;
    }

private:
    std::string name_;
    uint32_t version_;
    bool is_dirty_;

    std::vector<Symbol> public_types_;
    std::vector<Symbol> public_functions_;
    std::vector<Symbol> public_traits_;

    std::vector<Symbol> protected_types_;
    std::vector<Symbol> protected_functions_;
    std::vector<Symbol> protected_traits_;

    std::vector<Symbol> private_types_;
    std::vector<Symbol> private_functions_;
    std::vector<Symbol> private_traits_;

    std::vector<Change> changes_;
};

// ============================================================================
// Aliases for common usage
// ============================================================================

using ImportPtr = std::unique_ptr<Import>;

} // namespace import
} // namespace zith
#endif