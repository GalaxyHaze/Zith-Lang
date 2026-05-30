#pragma once

#include <cstdint>
#include <string_view>

namespace zith::backend::interface_ {

    enum class Arch : uint8_t {
        X86_64, Aarch64, Wasm32, Unknown
    };

    enum class OS : uint8_t {
        Linux, Macos, Windows, Unknown
    };

    enum class Endian : uint8_t {
        Little, Big
    };

    struct TargetInfo {
        Arch arch = Arch::Unknown;
        OS os = OS::Unknown;
        Endian endian = Endian::Little;
        uint32_t pointer_size = 8;

        static TargetInfo host();
    };

}
