#pragma once

#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "zir/hir/hir-expr.hpp"
#include "zir/hir/hir-types.hpp"

#include <string_view>

namespace zith::zir::hir {

struct HirBasicBlock {
    memory::DynArray<HirExprId> insts;
    HirExprId terminator = kInvalidHirExpr;

    explicit HirBasicBlock(memory::Arena &arena) : insts(arena) {}
};

struct HirFunction {
    std::string_view name;
    memory::DynArray<HirTypeId> params;
    HirTypeId return_type;
    memory::DynArray<HirBasicBlock> blocks;

    explicit HirFunction(memory::Arena &arena) : params(arena), blocks(arena) {}
};

class HirModule {
    memory::DynArray<HirExpr> exprs_;
    memory::DynArray<HirFunction> fns_;

public:
    explicit HirModule(memory::Arena &arena);
    HirModule(HirModule &&) = default;
    HirModule &operator=(HirModule &&) = default;

    HirExprId addExpr(HirExpr expr);
    HirFunction &addFn(std::string_view name);
    HirFunction &getFn(size_t idx);

    const HirExpr &getExpr(HirExprId id) const;
    const HirFunction &getFn(size_t idx) const;
    size_t getFnCount() const;
};

} // namespace zith::zir::hir
