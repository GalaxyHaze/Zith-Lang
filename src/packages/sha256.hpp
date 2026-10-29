#pragma once

#include <string>
#include <string_view>

namespace zith::packages {

[[nodiscard]] std::string sha256Hex(std::string_view input);

} // namespace zith::packages
