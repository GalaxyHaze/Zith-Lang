#pragma once

#include "import/symbol-table.hpp"
#include "memory/arena.hpp"
#include "zir/hir/hir-module.hpp"
#include "zir/zir/zir-inst.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace zith::zir {

class ZirEmitter {
    const hir::HirModule &hir_;
    ZirModule &out_;
    memory::Arena &arena_;
    const import::SymbolTable &syms_;

public:
    ZirEmitter(const hir::HirModule &hir, ZirModule &out, memory::Arena &arena,
               const import::SymbolTable &syms);

    void emit();

    const import::SymbolTable &getSyms() const noexcept {
        return syms_;
    }

    ZirIndex addConst(int64_t v);
    ZirIndex addConst(double v);
    ZirIndex addConst(std::string_view v);

private:
    void emitFn(const hir::HirFunction &fn);
};

} // namespace zith::zir
