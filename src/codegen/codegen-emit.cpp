#include "codegen-emit.hpp"

namespace zith::codegen {

CodeGenEmit::CodeGenEmit(llvm::IRBuilderBase &builder, CodeGenType &typeGen,
                         const memory::StringInterner &interner, const types::TypeIntern &types)
    : builder_(builder), typeGen_(typeGen), interner_(interner), types_(types) {}

} // namespace zith::codegen
