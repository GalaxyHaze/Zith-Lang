#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"

#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace zith::infra::interner {

    using InternedId = uint32_t;

    class StringInterner {
        infra::alloc::Arena *arena_;
        std::unordered_map<std::string_view, InternedId> map_;
        infra::collections::DynArray<const char *> strings_;

    public:
        explicit StringInterner(infra::alloc::Arena &arena);

        InternedId intern(std::string_view str);
        std::string_view lookup(InternedId id) const;
    };

} // namespace zith::infra::interner
