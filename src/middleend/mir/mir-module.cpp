#include "mir-module.hpp"

namespace zith::middleend::mir {

    MirModule::MirModule(infra::alloc::Arena& arena)
        : fns_(arena) {}

    MirFunction& MirModule::addFn(std::string_view name) {
        fns_.emplace();
        fns_[fns_.size() - 1].name = name;
        return fns_[fns_.size() - 1];
    }

    MirFunction& MirModule::getFn(size_t idx) { return fns_[idx]; }
    const MirFunction& MirModule::getFn(size_t idx) const { return fns_[idx]; }
    size_t MirModule::fnCount() const noexcept { return fns_.size(); }

}
