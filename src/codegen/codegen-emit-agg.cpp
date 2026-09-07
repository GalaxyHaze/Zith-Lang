#include "codegen-emit.hpp"

#include "common/overloaded.hpp"
#include "types/type-kind.hpp"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>

namespace zith::codegen {

llvm::Value *CodeGenEmit::emitAddrOf(hir::HirExprId id, const hir::HirModule &mod) {
    auto &operandExpr = mod.getExpr(id);
    if (auto *var = std::get_if<hir::HirVar>(&operandExpr))
        return emitVarAddr(*var);
    if (auto *slot_load = std::get_if<hir::HirSlotLoad>(&operandExpr)) {
        if (slot_load->slot >= slots_.size())
            return nullptr;
        return slots_[slot_load->slot];
    }
    if (auto *slot_addr = std::get_if<hir::HirSlotAddr>(&operandExpr)) {
        if (slot_addr->slot >= slots_.size())
            return nullptr;
        return slots_[slot_addr->slot];
    }
    if (auto *field = std::get_if<hir::HirField>(&operandExpr))
        return emitFieldAddr(*field, mod);
    if (auto *index = std::get_if<hir::HirIndex>(&operandExpr))
        return emitIndexAddr(*index, mod);
    if (auto *unary = std::get_if<hir::HirUnary>(&operandExpr)) {
        if (unary->op == hir::HirUnaryOp::Deref)
            return emitExpr(unary->operand, mod); // `*p` is addressed by the pointer itself
    }
    if (auto *union_cast = std::get_if<hir::HirUnionCast>(&operandExpr)) {
        if (types_.kindOf(union_cast->from) != types::TypeKind::Union &&
            types_.kindOf(union_cast->to) == types::TypeKind::Union) {
            auto *storage = builder_.CreateAlloca(typeGen_.lower(union_cast->to));
            builder_.CreateStore(llvm::ConstantAggregateZero::get(typeGen_.lower(union_cast->to)),
                                 storage);
            auto *value = emitExpr(union_cast->value, mod);
            if (value == nullptr)
                return nullptr;
            auto *bytes = builder_.CreateStructGEP(
                typeGen_.lower(union_cast->to),
                builder_.CreateBitCast(storage, llvm::PointerType::get(builder_.getContext(), 0)),
                0U);
            builder_.CreateStore(
                value,
                builder_.CreateBitCast(bytes, llvm::PointerType::get(builder_.getContext(), 0)));
            return storage;
        }
    }
    // Not directly addressable (e.g. a call result or literal): spill the value
    // into a temporary so its address can be taken.
    auto *value = emitExpr(id, mod);
    if (!value)
        return nullptr;
    auto *spill = builder_.CreateAlloca(value->getType());
    builder_.CreateStore(value, spill);
    return spill;
}

llvm::Value *CodeGenEmit::emitIndexAddr(const hir::HirIndex &idx, const hir::HirModule &mod) {
    // A slice is a `{ *T, i64 }` aggregate: index through its data pointer.
    if (idx.obj_type && types_.kindOf(idx.obj_type) == types::TypeKind::Slice) {
        auto *aggregate = emitExpr(idx.object, mod);
        auto *index_val = emitExpr(idx.index, mod);
        if (!aggregate || !index_val)
            return nullptr;
        auto *data = builder_.CreateExtractValue(aggregate, {0U});
        return builder_.CreateGEP(typeGen_.lower(idx.type), data, index_val);
    }

    llvm::Value *addr = nullptr;
    if (idx.is_array) {
        addr = emitAddrOf(idx.object, mod);
    } else {
        addr = emitExpr(idx.object, mod);
    }

    if (!addr)
        return nullptr;

    auto *index_val = emitExpr(idx.index, mod);
    if (!index_val)
        return nullptr;

    if (idx.is_array) {
        llvm::Value *zero = builder_.getInt32(0);
        auto *arr_type    = typeGen_.lower(idx.obj_type);
        return builder_.CreateGEP(arr_type, addr, {zero, index_val});
    } else {
        auto *elem_type = typeGen_.lower(idx.type);
        return builder_.CreateGEP(elem_type, addr, index_val);
    }
}

llvm::Value *CodeGenEmit::emitFieldAddr(const hir::HirField &field, const hir::HirModule &mod) {
    llvm::Value *base = emitAddrOf(field.object, mod);
    if (!base) {
        // Fall back to spilling the aggregate value so its fields can be addressed.
        auto *value = emitExpr(field.object, mod);
        if (!value)
            return nullptr;
        auto *spill = builder_.CreateAlloca(value->getType());
        builder_.CreateStore(value, spill);
        base = spill;
    }
    return builder_.CreateStructGEP(typeGen_.lower(field.object_type), base, field.index);
}

llvm::Value *CodeGenEmit::emitLValueAddr(hir::HirExprId target_id, const hir::HirModule &mod) {
    auto &expr = mod.getExpr(target_id);
    return hir::visitExpr(
        expr,
        common::overloaded{
            [&](const hir::HirVar &var) -> llvm::Value * { return emitVarAddr(var); },
            [&](const hir::HirSlotAddr &s) -> llvm::Value * {
                if (s.slot >= slots_.size())
                    return nullptr;
                return slots_[s.slot];
            },
            [&](const hir::HirIndex &idx) -> llvm::Value * { return emitIndexAddr(idx, mod); },
            [&](const hir::HirField &field) -> llvm::Value * { return emitFieldAddr(field, mod); },
            [&](const hir::HirUnary &un) -> llvm::Value * {
                if (un.op == hir::HirUnaryOp::Deref) {
                    return emitExpr(un.operand, mod);
                }
                return nullptr;
            },
            [](const auto &) -> llvm::Value * { return nullptr; }});
}

} // namespace zith::codegen
