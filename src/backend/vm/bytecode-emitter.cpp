#include "bytecode-emitter.hpp"

namespace zith::backend::vm {

    BytecodeEmitter::BytecodeEmitter(const middleend::mir::MirModule& mir)
        : mir_(mir) {}

    interface_::BackendResult BytecodeEmitter::emit() {
        return interface_::BackendResult{false, {}, {}, {}};
    }

}
