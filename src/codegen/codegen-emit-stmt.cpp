#include "codegen-emit.hpp"

#include "llvm/IR/CallingConv.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

namespace zith::codegen {

llvm::Value *CodeGenEmit::emitBody(const hir::HirFunction &fn, const hir::HirModule &mod) {
    if (fn.blocks.empty())
        return nullptr;
    if (!blocks_ || blocks_->empty())
        return nullptr;

    llvm::Value *last = nullptr;
    for (size_t i = 0; i < fn.blocks.size(); i++) {
        auto &block  = fn.blocks[i];
        auto *llvmBB = (*blocks_)[i];
        builder_.SetInsertPoint(llvmBB);
        emittedValues_.clear();

        for (auto inst_id : block.insts) {
            last = emitExpr(inst_id, mod);
        }
        if (block.terminator != hir::kInvalidHirExpr) {
            emitExpr(block.terminator, mod);
        }
    }
    return last;
}

llvm::Value *CodeGenEmit::emitStateTailCall(const hir::HirStateTailCall &tail,
                                            const hir::HirModule &mod) {
    if (tail.call.resolved_fn == symbols::kInvalidSym || tail.call.callee != hir::kInvalidHirExpr)
        return nullptr;

    llvm::Function *fn = nullptr;
    for (size_t i = 0; i < mod.getFnCount(); ++i) {
        if (mod.getFn(i).sym_id != tail.call.resolved_fn)
            continue;
        const auto name = interner_.lookup(mod.getFn(i).name);
        fn              = module_->getFunction(llvm::StringRef(name.data(), name.size()));
        break;
    }
    if (fn == nullptr)
        return nullptr;

    llvm::SmallVector<llvm::Value *, 8> args;
    for (auto arg_id : tail.call.args) {
        auto *value = emitExpr(arg_id, mod);
        if (value == nullptr)
            return nullptr;
        args.push_back(value);
    }

    const auto *current = builder_.GetInsertBlock();
    if (current == nullptr || current->getTerminator() != nullptr ||
        fn->arg_size() != args.size() || fn->isVarArg()) {
        return nullptr;
    }

    const auto *current_fn = current->getParent();
    if (current_fn == nullptr || fn->getReturnType() != current_fn->getReturnType()) {
        return nullptr;
    }

    auto *call = builder_.CreateCall(fn, args);
    call->setTailCallKind(llvm::CallInst::TCK_MustTail);
    call->setCallingConv(llvm::CallingConv::Tail);
    if (fn->getReturnType()->isVoidTy())
        return builder_.CreateRetVoid();
    return builder_.CreateRet(call);
}

llvm::Value *CodeGenEmit::emitCleanup(const hir::HirCleanup &cleanup, const hir::HirModule &mod) {
    llvm::Value *last = nullptr;
    for (const auto expr_id : cleanup.exprs) {
        auto *value = emitExpr(expr_id, mod);
        if (value != nullptr)
            last = value;
    }
    return last;
}

void CodeGenEmit::registerParams(const hir::HirFunction &fn, llvm::Function *llvmFn,
                                 const hir::HirModule &mod) {
    auto argIt = llvmFn->arg_begin();
    for (size_t i = 0; i < fn.param_names.size() && argIt != llvmFn->arg_end(); i++, ++argIt) {
        auto paramName = interner_.lookup(fn.param_names[i]);
        argIt->setName(llvm::StringRef(paramName.data(), paramName.size()));
        if (i < fn.param_slots.size()) {
            const auto *slotAttrs =
                mod.attrs().trySlot(static_cast<hir::HirSlotId>(fn.param_slots[i]));
            const bool borrow_attr =
                slotAttrs != nullptr && (slotAttrs->ownership == hir::HirOwnership::Lend ||
                                         slotAttrs->ownership == hir::HirOwnership::View);
            const bool is_pointer = argIt->getType()->isPointerTy();
            // Borrow facts only apply when the ABI passed a pointer for this
            // parameter. Generic/interface arguments can be by-value aggregates
            // with a residual qualifier; applying readonly/nocapture there is an
            // LLVM type error.
            if (borrow_attr && is_pointer) {
                argIt->addAttr(llvm::Attribute::getWithCaptureInfo(argIt->getContext(),
                                                                   llvm::CaptureInfo::none()));
                if (slotAttrs->ownership == hir::HirOwnership::View)
                    argIt->addAttr(llvm::Attribute::ReadOnly);
            }
        }
        auto *slot = builder_.CreateAlloca(argIt->getType(), nullptr,
                                           llvm::StringRef(paramName.data(), paramName.size()));
        builder_.CreateStore(&*argIt, slot);
        namedValues_[paramName] = {slot, argIt->getType(), true};
    }
}

llvm::Value *CodeGenEmit::emitRet(const hir::HirRet &ret, const hir::HirModule &mod) {
    if (ret.value == hir::kInvalidHirExpr)
        return builder_.CreateRetVoid();
    // The implicit-return path lowers the trailing expression both as an
    // instruction and as the Ret value. Reuse the value produced earlier in the
    // same block when available; the block-scoped cache avoids re-evaluating
    // calls while never reusing values from a different CFG edge.
    auto *val = emitExpr(ret.value, mod);
    if (!val)
        return nullptr;
    return builder_.CreateRet(val);
}

llvm::Value *CodeGenEmit::emitJump(const hir::HirJump &jump, const hir::HirModule &mod) {
    (void)mod;
    if (!blocks_ || jump.target >= blocks_->size())
        return nullptr;
    auto *target = (*blocks_)[jump.target];
    // Ordinary jumps are terminator-only edges between lowered CFG blocks.
    return builder_.CreateBr(target);
}

llvm::Value *CodeGenEmit::emitBranch(const hir::HirBranch &branch, const hir::HirModule &mod) {
    if (!blocks_ || branch.then_block >= blocks_->size() || branch.else_block >= blocks_->size())
        return nullptr;
    auto *condVal = emitExpr(branch.cond, mod);
    if (!condVal)
        return nullptr;
    auto *thenBB = (*blocks_)[branch.then_block];
    auto *elseBB = (*blocks_)[branch.else_block];
    return builder_.CreateCondBr(condVal, thenBB, elseBB);
}

llvm::Value *CodeGenEmit::emitVarAddr(const hir::HirVar &var) {
    auto name = interner_.lookup(var.name);
    auto *nv  = namedValues_.get(name);
    if (nv)
        return nv->value;
    if (module_ != nullptr) {
        if (auto *fn = module_->getFunction(llvm::StringRef(name.data(), name.size())))
            return fn;
    }
    return nullptr;
}

} // namespace zith::codegen
