#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "zir/mir/mir-inst.hpp"

#include <string_view>
#include <vector>

namespace zith::zir::mir {

    struct MirFunction {
        std::string_view name;
        std::vector<hir::HirTypeId> params;
        hir::HirTypeId return_type;
        std::vector<MirBasicBlock> blocks;
    };

    class MirModule {
        memory::DynArray<MirFunction> fns_;

    public:
        explicit MirModule(memory::Arena &arena);

        MirFunction &addFn(std::string_view name);
        MirFunction &getFn(size_t idx);
        const MirFunction &getFn(size_t idx) const;
        size_t fnCount() const noexcept;
    };

} // namespace zith::zir::mir
