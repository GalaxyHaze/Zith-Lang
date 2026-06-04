#pragma once

#include "backend/interface/backend-options.hpp"
#include "backend/interface/backend-result.hpp"
#include "middleend/mir/mir-module.hpp"

namespace zith::backend::interface_ {

    class Backend {
    public:
        virtual ~Backend() = default;

        virtual BackendResult compile(const middleend::mir::MirModule &module,
                                      const BackendOptions &opts) = 0;
        virtual const char *name() const noexcept                 = 0;
    };

} // namespace zith::backend::interface_
