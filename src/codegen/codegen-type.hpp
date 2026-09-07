#pragma once

#include "types/type-id.hpp"
#include "types/type-intern.hpp"

#include <llvm/IR/DataLayout.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace llvm {
class IRBuilderBase;
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
    /// ABI type used by C declarations/calls for a by-value record. Foreign
    /// records validated by the C binder can lower to one 64-bit integer;
    /// native Zith structs and unclassified records keep their storage type.
    llvm::Type *abiLower(types::TypeId id);
    /// Coerces an aggregate value to a foreign single-i64 C ABI value, or
    /// returns the value unchanged when the type has no such ABI.
    llvm::Value *abiCoerceArgument(llvm::IRBuilderBase &builder, llvm::Value *value,
                                   types::TypeId type);
    /// Coerces a foreign single-i64 C ABI return back to the aggregate storage
    /// type, or returns the value unchanged when the type has no such ABI.
    llvm::Value *abiRestoreResult(llvm::IRBuilderBase &builder, llvm::Value *value,
                                  types::TypeId type);
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
