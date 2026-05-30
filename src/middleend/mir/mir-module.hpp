#pragma once

#include "infra/alloc/arena.hpp"
#include "infra/collections/dyn-array.hpp"
#include "middleend/mir/mir-inst.hpp"

#include <string_view>
#include <vector>

namespace zith::middleend::mir {

    struct MirFunction {
        std::string_view name;
        std::vector<hir::HirTypeId> params;
        hir::HirTypeId return_type;
        std::vector<MirBasicBlock> blocks;
    };

    class MirModule {
        infra::collections::DynArray<MirFunction> fns_;

    public:
        explicit MirModule(infra::alloc::Arena &arena);

        MirFunction &addFn(std::string_view name);
        MirFunction &getFn(size_t idx);
        const MirFunction &getFn(size_t idx) const;
        size_t fnCount() const noexcept;
    };

} // namespace zith::middleend::mir
