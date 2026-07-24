#pragma once

#include <variant>

namespace zith::token {

template <typename T, typename V>
consteval auto is(const V &v) -> bool {
    return std::holds_alternative<T>(v);
}

template <typename T, typename V>
consteval auto as(const V &v) -> const T & {
    return std::get<T>(v);
}

} // namespace zith::token
