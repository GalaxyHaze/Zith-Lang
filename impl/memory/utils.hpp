#pragma once

#include <cstring>
#include <zith/memory.h>
#include <filesystem>
#include <fstream>

namespace zith {

template<typename T>
struct ArenaList {
    struct Chunk {
        Chunk *next;
        size_t len;
        size_t capacity;

        T *items() {
            return reinterpret_cast<T *>(this + 1);
        }

        const T *items() const {
            return reinterpret_cast<const T *>(this + 1);
        }
    };

    Chunk *head_ = nullptr;
    Chunk *tail_ = nullptr;
    size_t total_ = 0;
    size_t chunk_capacity_ = 0;

    void init(ZithArena * /*arena*/, size_t chunk_capacity) {
        head_ = nullptr;
        tail_ = nullptr;
        total_ = 0;
        chunk_capacity_ = chunk_capacity > 0 ? chunk_capacity : 16;
    }

    size_t size() const { return total_; }
    bool empty() const { return total_ == 0; }

    void push(ZithArena *arena, const T &value) {
        if (!tail_ || tail_->len == tail_->capacity) {
            alloc_chunk(arena);
        }
        tail_->items()[tail_->len++] = value;
        total_++;
    }

    T *flatten(ZithArena *arena, size_t *out_count) const {
        *out_count = total_;
        if (total_ == 0) return nullptr;

        T *arr = static_cast<T *>(zith_arena_alloc(arena, total_ * sizeof(T)));
        if (!arr) {
            *out_count = 0;
            return nullptr;
        }

        size_t i = 0;
        for (const Chunk *c = head_; c; c = c->next) {
            memcpy(arr + i, c->items(), c->len * sizeof(T));
            i += c->len;
        }
        return arr;
    }

    T *at(size_t index) {
        for (Chunk *c = head_; c; c = c->next) {
            if (index < c->len) return &c->items()[index];
            index -= c->len;
        }
        return nullptr;
    }

    const T *at(size_t index) const {
        for (const Chunk *c = head_; c; c = c->next) {
            if (index < c->len) return &c->items()[index];
            index -= c->len;
        }
        return nullptr;
    }

    struct Iterator {
        const Chunk *chunk;
        size_t index;

        bool operator!=(const Iterator &o) const {
            return chunk != o.chunk || index != o.index;
        }

        Iterator &operator++() {
            if (!chunk) return *this;
            if (++index >= chunk->len) {
                chunk = chunk->next;
                index = 0;
            }
            while (chunk && chunk->len == 0) { chunk = chunk->next; }
            return *this;
        }

        const T &operator*() const { return chunk->items()[index]; }
    };

    Iterator begin() const {
        const Chunk *c = head_;
        while (c && c->len == 0) c = c->next;
        return {c, 0};
    }

    Iterator end() const { return {nullptr, 0}; }

private:
    void alloc_chunk(ZithArena *arena) {
        const size_t alloc_size = sizeof(Chunk) + chunk_capacity_ * sizeof(T);
        auto *c = static_cast<Chunk *>(zith_arena_alloc(arena, alloc_size));
        if (!c) return;

        c->next = nullptr;
        c->len = 0;
        c->capacity = chunk_capacity_;

        if (tail_) tail_->next = c;
        else head_ = c;
        tail_ = c;
    }
};

inline bool create_file(const std::string& path_str, const std::string& content) {
    std::filesystem::path filepath(path_str);
    std::filesystem::path dir = filepath.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir)) {
        if (!std::filesystem::create_directories(dir)) {
            return false;
        }
    }

    std::ofstream file(filepath, std::ios::out | std::ios::binary);
    if (!file) return false;

    file << content;
    return true;
}

}
