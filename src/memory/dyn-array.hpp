#pragma once
#include "memory/arena.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace zith::memory {

class Arena;

template <typename T> class DynArray {
    Arena *arena    = nullptr;
    T *data         = nullptr;
    size_t len     = 0;
    size_t cap = 0;

public:
    explicit DynArray(Arena &arena) noexcept : arena(&arena) {}

    ~DynArray() noexcept {
        if (!data)
            return;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < len; i++) {
                data[i].~T();
            }
        }
    }

    DynArray(const DynArray &)       = delete;
    auto operator=(const DynArray &) = delete;

    DynArray(DynArray &&other) noexcept
        : arena(other.arena), data(other.data), len(other.len), cap(other.cap) {
        other.data     = nullptr;
        other.len     = 0;
        other.cap = 0;
    }

    auto operator=(DynArray &&other) noexcept -> DynArray & {
        if (this != &other) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = 0; i < len; i++) {
                    data[i].~T();
                }
            }
            arena          = other.arena;
            data           = other.data;
            len           = other.len;
            cap       = other.cap;
            other.data     = nullptr;
            other.len     = 0;
            other.cap = 0;
        }
        return *this;
    }

    void push(const T &value) {
        if (len >= cap) {
            grow_();
        }
        ::new (&data[len]) T(value);
        len++;
    }

    void push(T &&value) {
        if (len >= cap) {
            grow_();
        }
        ::new (&data[len]) T(std::move(value));
        len++;
    }

    template <typename... Args> auto &emplace(Args &&...args) {
        if (len >= cap) {
            grow_();
        }
        auto *ptr = ::new (&data[len]) T(std::forward<Args>(args)...);
        len++;
        return *ptr;
    }

    void reserve(size_t new_cap) {
        if (new_cap <= cap)
            return;

        size_t old_bytes = cap * sizeof(T);
        size_t new_bytes = new_cap * sizeof(T);
        size_t extra     = new_bytes - old_bytes;

        auto *dataend = reinterpret_cast<char *>(data) + old_bytes;
        if (dataend == arena->ptr() && arena->remaining() >= extra) {
            (void)arena->alloc(extra, alignof(T));
            cap = new_cap;
            return;
        }

        auto *new_data = static_cast<T *>(arena->alloc(new_bytes, alignof(T)));

        if (data) {
            for (size_t i = 0; i < len; i++) {
                ::new (&new_data[i]) T(std::move(data[i]));
                data[i].~T();
            }
        }

        data     = new_data;
        cap = new_cap;
    }

    [[nodiscard]] auto size() const noexcept -> size_t {
        return len;
    }
    [[nodiscard]] auto capacity() const noexcept -> size_t {
        return cap;
    }
    [[nodiscard]] auto empty() const noexcept -> bool {
        return len == 0;
    }

    void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < len; i++) {
                data[i].~T();
            }
        }
        len = 0;
    }

    void pop_back() {
        if (len == 0)
            return;
        if constexpr (!std::is_trivially_destructible_v<T>)
            data[len - 1].~T();
        len--;
    }

    auto back() noexcept -> T & {
        if (size() == 0) {
            std::fprintf(stderr, "[error] attemp to use 'back' on DynArray siz 0");
            std::abort();
        }
        return data[len - 1];
    }
    auto back() const noexcept -> const T & {
        if (size() == 0) {
            std::fprintf(stderr, "[error] attemp to use 'back' on DynArray siz 0");
            std::abort();
        }
        return data[len - 1];
    }

    auto operator[](size_t index) -> T & {
        if (index >= len) {
            std::fprintf(stderr, "[error] DynArray index out of bounds");
            std::abort();
        }
        return data[index];
    }
    auto operator[](size_t index) const -> const T & {
        if (index >= len) {
            std::fprintf(stderr, "[error] DynArray index out of bounds");
            std::abort();
        }
        return data[index];
    }

    auto begin() noexcept -> T * {
        return data;
    }
    auto end() noexcept -> T * {
        return data + len;
    }
    auto begin() const noexcept -> const T * {
        return data;
    }
    auto end() const noexcept -> const T * {
        return data + len;
    }

    auto getData() noexcept -> T * {
        return data;
    }
    auto getData() const noexcept -> const T * {
        return data;
    }

    auto getArena() noexcept -> Arena & {
        return *arena;
    }

private:
    void grow_() {
        size_t new_cap = cap == 0 ? 4 : cap * 2;
        reserve(new_cap);
    }
};

template <class T> auto makeDynArray(Arena &allocator) {
    return DynArray<T>(allocator);
}

} // namespace zith::memory
