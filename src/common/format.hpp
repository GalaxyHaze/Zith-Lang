#pragma once

#include <cstdio>
#include <string>
#include <utility>

namespace zith::common {

template <class... Args> std::string format(const char *fmt, Args &&...args) {
    int size = std::snprintf(nullptr, 0, fmt, std::forward<Args>(args)...);
    if (size <= 0)
        return {};

    std::string out(static_cast<size_t>(size) + 1, '\0');
    std::snprintf(out.data(), out.size(), fmt, std::forward<Args>(args)...);
    out.pop_back();
    return out;
}

} // namespace zith::common
