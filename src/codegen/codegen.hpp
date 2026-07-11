#pragma once

#include "diagnostics/diagnostic-engine.hpp"
#include "hir/hir-module.hpp"
#include "memory/string-interner.hpp"
#include "types/type-intern.hpp"

#include <string_view>

namespace zith::codegen {

// LLVM lowering currently produces an IR module and function declarations.
// Keeping the target here makes every later object/assembly emitter use the
// exact triple parsed by the CLI.
class CodeGenerator {
public:
    bool lowerToLlvm(const hir::HirModule &hir, const types::TypeIntern &types,
                     const memory::StringInterner &interner, std::string_view target_triple,
                     bool print_ir, diagnostics::DiagnosticEngine &diags) const;
};

} // namespace zith::codegen
