#include "codegen-type.hpp"

#include "types/type-kind.hpp"

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

namespace zith::codegen {

CodeGenType::CodeGenType(llvm::LLVMContext &ctx, const types::TypeIntern &types)
    : ctx_(ctx), types_(types) {}

llvm::Type *CodeGenType::lower(types::TypeId id) {
    if (id == types::kInvalidType || id == types::kErrorType)
        return llvm::Type::getVoidTy(ctx_);

    auto &data = types_.lookup(id);

    return std::visit(
        [this](const auto &t) -> llvm::Type * {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, types::TypeVoid>) {
                return llvm::Type::getVoidTy(ctx_);
            } else if constexpr (std::is_same_v<T, types::TypeBool>) {
                return llvm::Type::getInt1Ty(ctx_);
            } else if constexpr (std::is_same_v<T, types::TypeChar>) {
                return llvm::Type::getInt8Ty(ctx_);
            } else if constexpr (std::is_same_v<T, types::TypeInt>) {
                return llvm::Type::getIntNTy(ctx_, types::intWidthBits(t.width));
            } else if constexpr (std::is_same_v<T, types::TypeFloat>) {
                switch (t.width) {
                case types::FloatWidth::F32:
                    return llvm::Type::getFloatTy(ctx_);
                case types::FloatWidth::F64:
                    return llvm::Type::getDoubleTy(ctx_);
                default:
                    return llvm::Type::getDoubleTy(ctx_);
                }
            } else if constexpr (std::is_same_v<T, types::TypePtr>) {
                return llvm::PointerType::get(ctx_, 0);
            } else if constexpr (std::is_same_v<T, types::TypeArray>) {
                auto *elem = lower(t.elem);
                return llvm::ArrayType::get(elem, t.count);
            } else if constexpr (std::is_same_v<T, types::TypeFn>) {
                auto *ret = lower(t.ret);
                llvm::SmallVector<llvm::Type *, 8> params;
                for (size_t i = 0; i < t.param_count; i++)
                    params.push_back(lower(t.params[i]));
                return llvm::FunctionType::get(ret, params, false);
            } else if constexpr (std::is_same_v<T, types::TypeNever>) {
                return llvm::Type::getVoidTy(ctx_);
            } else if constexpr (std::is_same_v<T, types::TypeStruct> ||
                                 std::is_same_v<T, types::TypeEnum> ||
                                 std::is_same_v<T, types::TypeUnion>) {
                return llvm::PointerType::get(ctx_, 0);
            } else if constexpr (std::is_same_v<T, types::TypeOptional> ||
                                 std::is_same_v<T, types::TypeFailable>) {
                return llvm::PointerType::get(ctx_, 0);
            } else {
                return llvm::PointerType::get(ctx_, 0);
            }
        },
        data);
}

llvm::Type *CodeGenType::lowerPtr(types::TypeId pointee, bool is_mut) {
    (void)pointee;
    (void)is_mut;
    return llvm::PointerType::get(ctx_, 0);
}

} // namespace zith::codegen
