#pragma once

#include "hir/hir-module.hpp"
#include "memory/arena.hpp"
#include "memory/string-interner.hpp"
#include "types/type-intern.hpp"
#include "vm/typed-ir.hpp"

#include <string>

namespace zith::vm {

struct LowerResult {
    bool ok = false;
    std::string message;
};

/// Lowers the supported HIR subset into the portable v2 VM. The subset is
/// intentionally small: user functions and extern declarations, integer
/// literals and arithmetic, slots, direct calls, and a few built-in FFI
/// handlers. Higher-level stdlib formatting is not emulated yet.
auto lowerModule(const hir::HirModule &hir, const memory::StringInterner &interner,
                 const types::TypeIntern &types, memory::Arena &arena, Module &out)
    -> LowerResult;

} // namespace zith::vm
