#pragma once
#include "memory/arena.hpp"
#include "memory/flat-map.hpp"
#include "support/macros.hpp"
#include <cstdint>
#include <string_view>

namespace zith::memory {

using InternedId = uint32_t;

struct Arena;
template <class T> struct DynArray;

// String-Interner will be used to store literals
struct StringInterner {
    aSelf(StringInterner);

    explicit StringInterner(memory::Arena &arena);
    StringInterner() = default;
    StringInterner(Self &&other) noexcept
        : allocator_(std::exchange(other.allocator_, nullptr)),
          map(std::exchange(other.map, nullptr)),
          pool(std::exchange(other.pool, nullptr)) {}

    StringInterner(const Self &) = delete;

    auto &operator=(Self &&other) noexcept {
        if (this != &other) {
            allocator_ = std::exchange(other.allocator_, nullptr);
            map        = std::exchange(other.map, nullptr);
            pool       = std::exchange(other.pool, nullptr);
        }
        return *this;
    }
    auto operator=(const Self &) = delete;
    InternedId intern(std::string_view str);
    std::string_view lookup(InternedId id) const;

private:
    Arena *allocator_ = nullptr;
    FlatMap<std::string_view, InternedId> *map = nullptr;
    memory::DynArray<std::string_view> *pool  = nullptr;
    void init();
};

} // namespace zith::memory
