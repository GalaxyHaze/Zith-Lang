#include "hir-module.hpp"

namespace zith::hir {

HirModule::HirModule(memory::Arena &arena) : exprs_(arena), fns_(arena) {}

HirExprId HirModule::addExpr(HirExpr expr) {
    HirExprId id = static_cast<HirExprId>(exprs_.size());
    exprs_.push(std::move(expr));
    return id;
}

HirFunction &HirModule::addFn(std::string_view name) {
    fns_.emplace(exprs_.arena());
    fns_.back().name = name;
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

size_t HirModule::getFnCount() const {
    return fns_.size();
}

} // namespace zith::hir
