#pragma once
#include "memory/arena.hpp"
#include "memory/flat-map.hpp"
#include "support/macros.hpp"
#include <cstdint>
#include <shared_mutex>
#include <string_view>

namespace zith::memory {

using InternedId = uint32_t;

struct Arena;
template <class T> struct DynArray;

struct StringInterner {
    aSelf(StringInterner);

    StringInterner() = default;
    explicit StringInterner(memory::Arena &arena);
    StringInterner(const Self &) = delete;
    auto operator=(const Self &) = delete;

    InternedId intern(std::string_view str);
    std::string_view lookup(InternedId id) const;

private:
    Arena *allocator_ = nullptr;
    FlatMap<std::string_view, InternedId> *map = nullptr;
    memory::DynArray<std::string_view> *pool  = nullptr;
    mutable std::shared_mutex rwMutex_;

    void init();
};

} // namespace zith::memory
