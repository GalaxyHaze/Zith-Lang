#pragma once

#include "backend/interface/backend-result.hpp"
#include "backend/vm/bytecode-format.hpp"
#include "middleend/mir/mir-module.hpp"
#include <vector>

namespace zith::backend::vm {

    class BytecodeEmitter {
        const middleend::mir::MirModule& mir_;
        std::vector<uint8_t> buffer_;

    public:
        explicit BytecodeEmitter(const middleend::mir::MirModule& mir);

        interface_::BackendResult emit();
    };

}
