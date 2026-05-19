#pragma once

#include <format>
#include <string>

namespace zith::diag {

template <typename... Args>
[[nodiscard]] inline std::string diag_format(std::string_view fmt, Args&&... args) {
    return std::vformat(fmt, std::make_format_args(args...));
}

} // namespace zith::diag
