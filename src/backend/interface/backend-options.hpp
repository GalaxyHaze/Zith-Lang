#pragma once

#include <cstdint>

namespace zith::backend::interface_ {

    enum class OptLevel : uint8_t {
        O0, O1, O2, O3
    };

    enum class OutputFormat : uint8_t {
        Bytecode, Object, Asm
    };

    struct BackendOptions {
        OptLevel opt = OptLevel::O0;
        OutputFormat format = OutputFormat::Bytecode;
        bool debug_info = false;
    };

}
