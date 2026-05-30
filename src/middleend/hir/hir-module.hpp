#pragma once

#include "infra/alloc/arena.hpp"
#include "infra/collections/dyn-array.hpp"
#include "middleend/hir/hir-expr.hpp"
#include "middleend/hir/hir-types.hpp"
#include <string_view>

namespace zith::middleend::hir {

    struct HirBasicBlock {
        infra::collections::DynArray<HirExprId> insts;
        HirExprId terminator = kInvalidHirExpr;
    };

    struct HirFunction {
        std::string_view name;
        std::vector<HirTypeId> params;
        HirTypeId return_type;
        std::vector<HirBasicBlock> blocks;
    };

    class HirModule {
        infra::collections::DynArray<HirExpr> exprs_;
        std::vector<HirFunction> fns_;

    public:
        explicit HirModule(infra::alloc::Arena& arena);

        HirExprId addExpr(HirExpr expr);
        HirFunction& addFn(std::string_view name);
        HirFunction& getFn(size_t idx);

        const HirExpr& getExpr(HirExprId id) const;
        const HirFunction& getFn(size_t idx) const;
    };

}
