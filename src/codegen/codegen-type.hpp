#pragma once

#include "types/type-id.hpp"
#include "types/type-intern.hpp"

namespace llvm {
class LLVMContext;
class Type;
} // namespace llvm

namespace zith::codegen {

class CodeGenType {
public:
    CodeGenType(llvm::LLVMContext &ctx, const types::TypeIntern &types);

    llvm::Type *lower(types::TypeId id);
    llvm::Type *lowerPtr(types::TypeId pointee, bool is_mut);

private:
    llvm::LLVMContext &ctx_;
    const types::TypeIntern &types_;
};

} // namespace zith::codegen
