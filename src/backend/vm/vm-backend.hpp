#pragma once

#include "backend/interface/backend.hpp"
#include "backend/vm/bytecode-emitter.hpp"

namespace zith::backend::vm {

    class VmBackend : public interface_::Backend {
    public:
        interface_::BackendResult compile(const middleend::mir::MirModule &module,
                                          const interface_::BackendOptions &opts) override;
        const char *name() const noexcept override;
    };

} // namespace zith::backend::vm
