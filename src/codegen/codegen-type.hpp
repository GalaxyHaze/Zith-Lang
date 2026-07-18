#pragma once

#include "types/type-id.hpp"
#include "types/type-intern.hpp"

#include <llvm/IR/DataLayout.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace llvm {
class LLVMContext;
class Type;
} // namespace llvm

namespace zith::codegen {

class CodeGenType {
public:
    CodeGenType(llvm::LLVMContext &ctx, const types::TypeIntern &types,
                const llvm::DataLayout *layout = nullptr);

    llvm::Type *lower(types::TypeId id);
    llvm::Type *lowerPtr(types::TypeId pointee, bool is_mut);
    uint64_t sizeOf(types::TypeId id) const;
    uint64_t alignOf(types::TypeId id) const;
    uint64_t fieldOffset(types::TypeId struct_type, size_t field_index) const;
    uint64_t fieldOffset(types::TypeId struct_type, std::string_view field_name) const;
    bool hasLayout() const noexcept {
        return layout_ != nullptr;
    }

private:
    llvm::LLVMContext &ctx_;
    const types::TypeIntern &types_;
    const llvm::DataLayout *layout_;
};

std::optional<llvm::DataLayout> makeTargetDataLayout(std::string_view target_triple);

} // namespace zith::codegen
