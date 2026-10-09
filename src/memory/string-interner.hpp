#pragma once
#include "memory/arena.hpp"
#include "memory/flat-map.hpp"
#include "support/macros.hpp"
#include <cstdint>
#if !defined(ZITH_IS_WASM)
#include <shared_mutex>
#endif
#include <string_view>

namespace zith::memory {

using InternedId = uint32_t;

inline constexpr InternedId kInvalidInternedId = ~InternedId{0};

class Arena;
template <class T> class DynArray;

struct StringInterner {
    aSelf(StringInterner);

    StringInterner() = default;
    explicit StringInterner(memory::Arena &arena);
    StringInterner(const Self &) = delete;
    auto operator=(const Self &) = delete;

    InternedId intern(std::string_view str);
    std::string_view lookup(InternedId id) const;
    [[nodiscard]] std::size_t poolSize() const noexcept;
    /// Internal implementation used by `intern` and `copyString` after the
    /// caller has taken the mutex.
    InternedId pushCopy(std::string_view str);
    /// Copies a previously interned string into this interner and returns the
    /// new id. Used by flat HIR decoding to rebuild positional string pools
    /// before `TypeIntern` has been constructed.
    InternedId copyString(std::string_view str);

private:
    Arena *allocator_                          = nullptr;
    FlatMap<std::string_view, InternedId> *map = nullptr;
    memory::DynArray<std::string_view> *pool   = nullptr;
#if !defined(ZITH_IS_WASM)
    mutable std::shared_mutex rwMutex_;
#endif

    void init();
};

} // namespace zith::memory
