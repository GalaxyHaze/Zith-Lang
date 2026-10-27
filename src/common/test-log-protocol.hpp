#pragma once

#include <cstddef>
#include <string_view>

namespace zith::common {

inline constexpr std::string_view kTestLogBeginPrefix = "\x1eZITH-TEST-LOG-BEGIN:";
inline constexpr std::string_view kTestLogEndMarker   = "\x1eZITH-TEST-LOG-END\x1e\n";
inline constexpr std::string_view kTestLogContextPrefix = "\x1eZITH-TEST-CONTEXT:";
inline constexpr std::size_t kTestLogCapacity        = 4096U;

} // namespace zith::common
