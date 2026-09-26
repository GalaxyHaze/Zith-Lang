#pragma once

#include "hir/hir-module.hpp"
#include "ir/exec-ir.hpp"
#include "memory/string-interner.hpp"
#include "types/type-intern.hpp"

namespace zith::ir {

struct LowerResult {
    bool ok = false;
    std::string message;
};

LowerResult lowerModule(const hir::HirModule &hir, const memory::StringInterner &interner,
                        const types::TypeIntern &types, memory::Arena &arena, Module &out);

} // namespace zith::ir
