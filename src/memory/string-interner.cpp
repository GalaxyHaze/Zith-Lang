#include "memory/string-interner.hpp"

#include <cstring>

namespace zith::memory {

    StringInterner::StringInterner(memory::Arena &arena) : arena_(&arena), strings_(arena) {}

    InternedId StringInterner::intern(std::string_view str) {
        auto it = map_.find(str);
        if (it != map_.end())
            return it->second;

        auto *copy = static_cast<char *>(arena_->alloc(str.size() + 1, 1));
        std::memcpy(copy, str.data(), str.size());
        copy[str.size()] = '\0';

        InternedId id = static_cast<InternedId>(strings_.size());
        strings_.push(copy);
        map_[strings_[id]] = id;
        return id;
    }

    std::string_view StringInterner::lookup(InternedId id) const {
        if (id >= strings_.size())
            return {};
        return strings_[id];
    }

} // namespace zith::memory
