#include "vm-backend.hpp"

namespace zith::backend::vm {

    interface_::BackendResult VmBackend::compile(const middleend::mir::MirModule &module,
                                                 const interface_::BackendOptions &opts) {
        (void)opts;
        BytecodeEmitter emitter(module);
        return emitter.emit();
    }

    const char *VmBackend::name() const noexcept {
        return "vm";
    }

} // namespace zith::backend::vm
