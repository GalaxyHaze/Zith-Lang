#include "hir-module.hpp"

namespace zith::zir::hir {

    HirModule::HirModule(memory::Arena &arena) : exprs_(arena) {}

    HirExprId HirModule::addExpr(HirExpr expr) {
        HirExprId id = static_cast<HirExprId>(exprs_.size());
        exprs_.push(std::move(expr));
        return id;
    }

    HirFunction &HirModule::addFn(std::string_view name) {
        (void)name;
        fns_.emplace_back();
        return fns_.back();
    }

    HirFunction &HirModule::getFn(size_t idx) {
        return fns_[idx];
    }
    const HirExpr &HirModule::getExpr(HirExprId id) const {
        return exprs_[id];
    }
    const HirFunction &HirModule::getFn(size_t idx) const {
        return fns_[idx];
    }

} // namespace zith::zir::hir
