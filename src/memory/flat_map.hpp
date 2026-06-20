#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace zith::memory {

struct TransparentStringHash {
    using is_transparent = void;
    size_t operator()(std::string_view sv) const noexcept { return std::hash<std::string_view>{}(sv); }
    size_t operator()(const std::string &s) const noexcept { return std::hash<std::string_view>{}(s); }
};

enum : uint8_t { FM_EMPTY = 0, FM_OCCUPIED = 1, FM_ERASED = 2 };

template <typename Key, typename Value, typename Hash = std::hash<Key>, typename Eq = std::equal_to<Key>>
class FlatMap {
    static_assert(std::is_same_v<Key, std::string> || std::is_same_v<Key, std::string_view> ||
                  std::is_arithmetic_v<Key> || std::is_enum_v<Key> || std::is_pointer_v<Key>,
                  "FlatMap supports string, string_view, arithmetic, enum, and pointer keys");

    static constexpr bool has_transparent_hash = requires { typename Hash::is_transparent; };
    static constexpr bool has_transparent_eq   = requires { typename Eq::is_transparent; };
    static constexpr bool is_transparent = has_transparent_hash && has_transparent_eq;

public:
    FlatMap() = default;
    explicit FlatMap(size_t initial_capacity) { reserve(initial_capacity); }

    FlatMap(const FlatMap &o)
        : meta_(o.meta_), keys_(o.keys_), values_(o.values_),
          size_(o.size_), capacity_mask_(o.capacity_mask_) {}
    FlatMap &operator=(const FlatMap &o) {
        if (this != &o) {
            meta_ = o.meta_; keys_ = o.keys_; values_ = o.values_;
            size_ = o.size_; capacity_mask_ = o.capacity_mask_;
        }
        return *this;
    }
    FlatMap(FlatMap &&o) noexcept
        : meta_(std::move(o.meta_)), keys_(std::move(o.keys_)),
          values_(std::move(o.values_)), size_(o.size_), capacity_mask_(o.capacity_mask_)
    { o.size_ = 0; o.capacity_mask_ = 0; }
    FlatMap &operator=(FlatMap &&o) noexcept {
        meta_ = std::move(o.meta_); keys_ = std::move(o.keys_);
        values_ = std::move(o.values_); size_ = o.size_; capacity_mask_ = o.capacity_mask_;
        o.size_ = 0; o.capacity_mask_ = 0; return *this;
    }

    void reserve(size_t n) {
        if (n <= capacity()) return;
        size_t cap = 1;
        while (cap < n) cap <<= 1;
        rehash(cap);
    }

    Value &insert(const Key &key, Value value) {
        if (size_ >= capacity() * 7 / 10) rehash(capacity() ? capacity() * 2 : 16);
        size_t idx = hash_to_index(key), start = idx;
        while (meta_[idx] == FM_OCCUPIED) {
            if (eq_(keys_[idx], key)) { values_[idx] = std::move(value); return values_[idx]; }
            idx = (idx + 1) & capacity_mask_;
            if (idx == start) { rehash(capacity() * 2); return insert(key, std::move(value)); }
        }
        keys_[idx] = key; meta_[idx] = FM_OCCUPIED; size_++;
        values_[idx] = std::move(value);
        return values_[idx];
    }

    Value &insert(Key &&key, Value value) {
        if (size_ >= capacity() * 7 / 10) rehash(capacity() ? capacity() * 2 : 16);
        size_t idx = hash_to_index(key), start = idx;
        while (meta_[idx] == FM_OCCUPIED) {
            if (eq_(keys_[idx], key)) { values_[idx] = std::move(value); return values_[idx]; }
            idx = (idx + 1) & capacity_mask_;
            if (idx == start) { rehash(capacity() * 2); return insert(std::move(key), std::move(value)); }
        }
        keys_[idx] = std::move(key); meta_[idx] = FM_OCCUPIED; size_++;
        values_[idx] = std::move(value);
        return values_[idx];
    }

    Value *get(const Key &key) {
        if (empty()) return nullptr;
        size_t idx = hash_to_index(key), start = idx;
        while (meta_[idx] != FM_EMPTY) {
            if (meta_[idx] == FM_OCCUPIED && eq_(keys_[idx], key)) return &values_[idx];
            idx = (idx + 1) & capacity_mask_; if (idx == start) break;
        }
        return nullptr;
    }
    const Value *get(const Key &key) const { return const_cast<FlatMap *>(this)->get(key); }

    template <typename LK> requires (is_transparent && !std::is_same_v<std::decay_t<LK>, Key>)
    Value *get(LK &&key) {
        if (empty()) return nullptr;
        size_t idx = hash_to_index(std::forward<LK>(key)), start = idx;
        while (meta_[idx] != FM_EMPTY) {
            if (meta_[idx] == FM_OCCUPIED && eq_(keys_[idx], key)) return &values_[idx];
            idx = (idx + 1) & capacity_mask_; if (idx == start) break;
        }
        return nullptr;
    }
    template <typename LK> requires (is_transparent && !std::is_same_v<std::decay_t<LK>, Key>)
    const Value *get(LK &&key) const { return const_cast<FlatMap *>(this)->get(std::forward<LK>(key)); }

    bool erase(const Key &key) {
        if (empty()) return false;
        size_t idx = hash_to_index(key), start = idx;
        while (meta_[idx] != FM_EMPTY) {
            if (meta_[idx] == FM_OCCUPIED && eq_(keys_[idx], key)) { meta_[idx] = FM_ERASED; size_--; return true; }
            idx = (idx + 1) & capacity_mask_; if (idx == start) break;
        }
        return false;
    }

    bool contains(const Key &key) const { return get(key) != nullptr; }
    template <typename LK> requires (is_transparent && !std::is_same_v<std::decay_t<LK>, Key>)
    bool contains(LK &&key) const { return get(std::forward<LK>(key)) != nullptr; }

    Value &operator[](const Key &key) {
        Value *v = get(key); if (v) return *v; return insert(key, Value{});
    }
    template <typename LK> requires (is_transparent && !std::is_same_v<std::decay_t<LK>, Key>)
    Value &operator[](LK &&key) {
        Value *v = get(std::forward<LK>(key)); if (v) return *v;
        return insert(Key(std::forward<LK>(key)), Value{});
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_mask_ ? capacity_mask_ + 1 : 0; }
    bool empty() const { return size_ == 0; }

    void clear() {
        if (meta_.empty()) return;
        for (size_t i = 0; i <= capacity_mask_; ++i)
            if (meta_[i] == FM_OCCUPIED) meta_[i] = FM_EMPTY;
        size_ = 0;
    }

    template <typename Fn> void for_each(Fn &&fn) const {
        size_t cap = capacity();
        for (size_t i = 0; i < cap; ++i)
            if (meta_[i] == FM_OCCUPIED) fn(keys_[i], values_[i]);
    }

    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key, Value>;
        using difference_type = std::ptrdiff_t;
        iterator() : map_(nullptr), idx_(0) {}
        iterator(FlatMap *map, size_t idx) : map_(map), idx_(idx) {}
        value_type operator*() const { return {map_->keys_[idx_], map_->values_[idx_]}; }
        const Key &key() const { return map_->keys_[idx_]; }
        Value &value() { return map_->values_[idx_]; }
        const Value &value() const { return map_->values_[idx_]; }
        iterator &operator++() { ++idx_; while (idx_ <= map_->capacity_mask_ && map_->meta_[idx_] != FM_OCCUPIED) ++idx_; return *this; }
        bool operator==(const iterator &o) const { return idx_ == o.idx_ && map_ == o.map_; }
        bool operator!=(const iterator &o) const { return !(*this == o); }
    private:
        friend class FlatMap;
        FlatMap *map_; size_t idx_;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key, const Value>;
        using difference_type = std::ptrdiff_t;
        const_iterator() : map_(nullptr), idx_(0) {}
        const_iterator(const FlatMap *map, size_t idx) : map_(map), idx_(idx) {}
        value_type operator*() const { return {map_->keys_[idx_], map_->values_[idx_]}; }
        const Key &key() const { return map_->keys_[idx_]; }
        const Value &value() const { return map_->values_[idx_]; }
        const_iterator &operator++() { ++idx_; while (idx_ <= map_->capacity_mask_ && map_->meta_[idx_] != FM_OCCUPIED) ++idx_; return *this; }
        bool operator==(const const_iterator &o) const { return idx_ == o.idx_ && map_ == o.map_; }
        bool operator!=(const const_iterator &o) const { return !(*this == o); }
    private:
        friend class FlatMap;
        const FlatMap *map_; size_t idx_;
    };

    iterator begin() { if (empty()) return end(); size_t idx = 0; while (idx <= capacity_mask_ && meta_[idx] != FM_OCCUPIED) ++idx; return iterator(this, idx); }
    iterator end() { return iterator(this, capacity_mask_ + 1); }
    const_iterator begin() const { if (empty()) return end(); size_t idx = 0; while (idx <= capacity_mask_ && meta_[idx] != FM_OCCUPIED) ++idx; return const_iterator(this, idx); }
    const_iterator end() const { return const_iterator(this, capacity_mask_ + 1); }

    bool erase(iterator it) {
        if (it == end()) return false;
        meta_[it.idx_] = FM_ERASED; size_--;
        return true;
    }

    iterator find(const Key &key) {
        if (empty()) return end();
        size_t idx = hash_to_index(key), start = idx;
        while (meta_[idx] != FM_EMPTY) {
            if (meta_[idx] == FM_OCCUPIED && eq_(keys_[idx], key)) return iterator(this, idx);
            idx = (idx + 1) & capacity_mask_; if (idx == start) break;
        }
        return end();
    }
    const_iterator find(const Key &key) const {
        if (empty()) return end();
        size_t idx = hash_to_index(key), start = idx;
        while (meta_[idx] != FM_EMPTY) {
            if (meta_[idx] == FM_OCCUPIED && eq_(keys_[idx], key)) return const_iterator(this, idx);
            idx = (idx + 1) & capacity_mask_; if (idx == start) break;
        }
        return end();
    }

private:
    template <typename LK> size_t hash_to_index(LK &&key) const {
        size_t h = hash_(std::forward<LK>(key));
        h *= 11400714819323198485ULL;
        return h & capacity_mask_;
    }

    void rehash(size_t new_cap) {
        auto old_meta = std::move(meta_);
        auto old_keys = std::move(keys_);
        auto old_values = std::move(values_);
        size_t old_cap = capacity();
        meta_.assign(new_cap, FM_EMPTY);
        keys_.resize(new_cap); values_.resize(new_cap);
        capacity_mask_ = new_cap - 1; size_ = 0;
        for (size_t i = 0; i < old_cap; ++i)
            if (old_meta[i] == FM_OCCUPIED)
                insert(std::move(old_keys[i]), std::move(old_values[i]));
    }

    Hash hash_{};
    Eq eq_{};
    std::vector<uint8_t> meta_;
    std::vector<Key> keys_;
    std::vector<Value> values_;
    size_t size_ = 0;
    size_t capacity_mask_ = 0;
};

} // namespace zith::memory
