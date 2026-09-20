#include "sema/hir-lower-modern.hpp"

#include "common/overloaded.hpp"
#include "diagnostics/error-codes.hpp"
#include "sema/hir-lower-utils.hpp"
#include "sema/op-mapping.hpp"
#include "support/int-literal.hpp"
#include "types/type-kind.hpp"

namespace zith::sema {
namespace modern {

hir::HirExprId HirLowerModern::lowerLValueAddr(frontend::ExprId id) {
    if (!id || current_module_ == nullptr || current_module_->frontend == nullptr ||
        id.value > current_module_->frontend->expressions().size())
        return hir::kInvalidHirExpr;
    const auto &expr = current_module_->frontend->expressions()[id.value - 1U];
    if (expr.kind == frontend::ExprKind::Name) {
        if (const auto *resolved = findResolvedExpr(id); resolved != nullptr && resolved->local) {
            const auto slot     = localSlot(resolved->local);
            const auto local_ty = typeOfLocal(resolved->local);
            for (auto it = narrowing_stack_.rbegin(); it != narrowing_stack_.rend(); ++it) {
                if (it->local == resolved->local && it->optionalPayload) {
                    const auto payload_field = addExpr(hir::HirField{
                        addExpr(hir::HirSlotAddr{slot, local_ty}), 0U, it->type, local_ty});
                    return addExpr(hir::HirUnary{hir::HirUnaryOp::Ref, payload_field,
                                                 types_.internPtr(it->type)});
                }
                if (it->local == resolved->local && it->opaquePayload) {
                    // A narrowed opaque value is still stored as `{ *void,
                    // typeId }`. Spill the unchecked payload extraction into a
                    // temporary so its address has the narrowed concrete type.
                    const auto temp_slot = next_slot_++;
                    current_fn_->blocks[current_block_].insts.push(
                        emitSlotAlloca(temp_slot, it->type));
                    hir::HirOpaqueCast cast;
                    cast.value        = emitSlotLoad(slot, local_ty);
                    cast.from         = local_ty;
                    cast.to           = it->type;
                    cast.opaque_type  = local_ty;
                    cast.result_type  = it->type;
                    cast.canonical_id = canonicalTypeId(it->type);
                    cast.type_id      = runtimeTagForCanonicalType(cast.canonical_id);
                    cast.checked      = false;
                    current_fn_->blocks[current_block_].insts.push(
                        emitSlotStore(temp_slot, addExpr(std::move(cast))));
                    return addExpr(hir::HirSlotAddr{temp_slot, it->type});
                }
            }
            return addExpr(hir::HirSlotAddr{slot, local_ty});
        }
        return hir::kInvalidHirExpr;
    }
    if (expr.kind == frontend::ExprKind::Field || expr.kind == frontend::ExprKind::Index) {
        if (current_types_ != nullptr && expr.kind == frontend::ExprKind::Field) {
            if (const auto *base = current_types_->traitQualifiedReceiverBase.get(expr.id.value))
                return lowerLValueAddr(frontend::ExprId{*base});
        }
        const auto value = lowerExpr(id);
        if (value == hir::kInvalidHirExpr)
            return hir::kInvalidHirExpr;
        hir::HirUnary unary;
        unary.op      = hir::HirUnaryOp::Ref;
        unary.operand = value;
        unary.type    = types_.internPtr(typeOfExpr(id));
        return addExpr(std::move(unary));
    }
    if (expr.kind == frontend::ExprKind::Unary && expr.text == "*" && !expr.operands.empty())
        return lowerExpr(expr.operands[0]);
    return hir::kInvalidHirExpr;
}
hir::HirExprId HirLowerModern::lowerIndex(const frontend::Expression &expr,
                                          const types::TypeId type) {
    if (expr.operands.size() < 2U)
        return hir::kInvalidHirExpr;
    const auto object = lowerExpr(expr.operands[0]);
    const auto index  = lowerExpr(expr.operands[1]);
    if (object == hir::kInvalidHirExpr || index == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    const auto object_type = typeOfExpr(expr.operands[0]);
    const auto sema_object = sema_.typeTable().stripQualifiers(semaTypeOfExpr(expr.operands[0]));
    if (sema_.typeTable().kindOf(sema_object) == sema::modern::TypeKind::Pack) {
        const auto *pack = sema_.typeTable().pack(sema_object);
        if (pack != nullptr) {
            int64_t index_value     = 0;
            const auto *module_sema = sema_.findModuleSema(current_module_->key);
            if (module_sema == nullptr ||
                !module_sema->constantIntegerValue(expr.operands[1], index_value) ||
                index_value < 0 || static_cast<uint64_t>(index_value) >= pack->members.size())
                return hir::kInvalidHirExpr;
            return addExpr(
                hir::HirField{object, static_cast<uint32_t>(index_value), type, object_type});
        }
    }
    hir::HirIndex indexing;
    indexing.object   = object;
    indexing.index    = index;
    indexing.type     = type;
    indexing.obj_type = object_type;
    indexing.is_array = types_.kindOf(object_type) == types::TypeKind::Array;
    if (expr.is_raw)
        return addExpr(std::move(indexing));

    const auto *optional    = std::get_if<types::TypeOptional>(&types_.lookup(type));
    const auto element_type = optional != nullptr ? optional->inner : type;
    const auto index_type   = typeOfExpr(expr.operands[1]);
    if (optional == nullptr)
        return addExpr(std::move(indexing));

    // Evaluate the object and index once, then branch on the dynamic bounds checks.
    const auto object_slot = next_slot_++;
    const auto index_slot  = next_slot_++;
    const auto result_slot = next_slot_++;
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(object_slot, object_type));
    current_fn_->blocks[current_block_].insts.push(emitSlotStore(object_slot, object));
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(
        index_slot,
        types_.kindOf(index_type) == types::TypeKind::Int ? index_type : types::kErrorType));
    current_fn_->blocks[current_block_].insts.push(emitSlotStore(index_slot, index));
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(result_slot, type));

    const auto loaded_index = emitSlotLoad(index_slot, index_type);
    hir::HirLiteral zero;
    zero.type          = index_type;
    zero.i             = 0;
    hir::HirExprId len = hir::kInvalidHirExpr;
    const auto *array  = std::get_if<types::TypeArray>(&types_.lookup(object_type));
    if (array != nullptr) {
        hir::HirLiteral len_lit;
        len_lit.type = index_type;
        len_lit.i    = static_cast<int64_t>(array->count);
        len          = addExpr(std::move(len_lit));
    } else {
        const auto loaded = emitSlotLoad(object_slot, object_type);
        auto len_i64 =
            addExpr(hir::HirField{loaded, 1U, types_.internInt(types::IntWidth::I64), object_type});
        const auto len64 = types_.internInt(types::IntWidth::I64);
        if (types_.kindOf(index_type) != types::TypeKind::Int) {
            len = hir::kInvalidHirExpr;
        } else if (index_type != len64) {
            len = addExpr(hir::HirCast{len_i64, len64, index_type});
        } else {
            len = len_i64;
        }
    }
    if (len == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    hir::HirBinary ge_zero;
    ge_zero.lhs           = loaded_index;
    ge_zero.rhs           = addExpr(std::move(zero));
    ge_zero.op            = hir::HirBinaryOp::Ge;
    ge_zero.type          = types::kBoolType;
    ge_zero.operand_type  = index_type;
    const auto ge_zero_id = addExpr(std::move(ge_zero));

    hir::HirBinary lt_len;
    lt_len.lhs           = loaded_index;
    lt_len.rhs           = len;
    lt_len.op            = hir::HirBinaryOp::Lt;
    lt_len.type          = types::kBoolType;
    lt_len.operand_type  = index_type;
    const auto lt_len_id = addExpr(std::move(lt_len));

    hir::HirBinary all_ok;
    all_ok.lhs          = ge_zero_id;
    all_ok.rhs          = lt_len_id;
    all_ok.op           = hir::HirBinaryOp::And;
    all_ok.type         = types::kBoolType;
    all_ok.operand_type = types::kBoolType;

    const auto some_block  = newBlock();
    const auto none_block  = newBlock();
    const auto merge_block = newBlock();
    hir::HirBranch branch;
    branch.cond       = addExpr(std::move(all_ok));
    branch.then_block = static_cast<hir::HirDeclId>(some_block);
    branch.else_block = static_cast<hir::HirDeclId>(none_block);
    setTerminator(addExpr(std::move(branch)));

    setCurrentBlock(some_block);
    current_fn_->blocks[some_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    {
        hir::HirIndex ok_index;
        ok_index.object   = emitSlotLoad(object_slot, object_type);
        ok_index.index    = emitSlotLoad(index_slot, index_type);
        ok_index.type     = element_type;
        ok_index.obj_type = object_type;
        ok_index.is_array = indexing.is_array;
        hir::HirMakeSome some;
        some.type  = type;
        some.value = addExpr(std::move(ok_index));
        current_fn_->blocks[some_block].insts.push(
            emitSlotStore(result_slot, addExpr(std::move(some))));
        emitJump(merge_block);
    }

    setCurrentBlock(none_block);
    current_fn_->blocks[none_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    {
        hir::HirMakeNone none;
        none.type = type;
        current_fn_->blocks[none_block].insts.push(
            emitSlotStore(result_slot, addExpr(std::move(none))));
        emitJump(merge_block);
    }

    setCurrentBlock(merge_block);
    current_fn_->blocks[merge_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    return emitSlotLoad(result_slot, type);
}
hir::HirExprId HirLowerModern::lowerSliceRange(const frontend::Expression &expr,
                                               const types::TypeId type) {
    if (expr.operands.size() < 3U)
        return hir::kInvalidHirExpr;
    const auto object = lowerExpr(expr.operands[0]);
    const auto lo     = lowerExpr(expr.operands[1]);
    const auto hi     = lowerExpr(expr.operands[2]);
    if (object == hir::kInvalidHirExpr || lo == hir::kInvalidHirExpr ||
        hi == hir::kInvalidHirExpr) {
        return hir::kInvalidHirExpr;
    }

    const auto object_type = typeOfExpr(expr.operands[0]);
    const auto *optional   = std::get_if<types::TypeOptional>(&types_.lookup(type));
    const auto slice_type  = optional != nullptr ? optional->inner : type;
    const auto bound_type  = typeOfExpr(expr.operands[1]);
    hir::HirMakeSlice slice;
    slice.object                           = object;
    slice.lo                               = lo;
    slice.hi                               = hi;
    slice.type                             = slice_type;
    slice.object_type                      = object_type;
    slice.bound_type                       = bound_type;
    slice.is_array                         = types_.kindOf(object_type) == types::TypeKind::Array;
    const sema::modern::TypeId sema_object = semaTypeOfExpr(expr.operands[0]);
    const auto *sema_optional =
        sema_.typeTable().optional(sema_.typeTable().stripQualifiers(sema_object));
    slice.is_pointer =
        types_.kindOf(object_type) == types::TypeKind::Ptr ||
        (sema_optional != nullptr && sema_.typeTable().pointer(sema_.typeTable().stripQualifiers(
                                         sema_optional->inner)) != nullptr);
    slice.checked = false;
    if (optional == nullptr || expr.is_raw)
        return addExpr(std::move(slice));

    // Evaluate the object and both bounds once, then branch on the dynamic checks.
    const auto object_slot = next_slot_++;
    const auto lo_slot     = next_slot_++;
    const auto hi_slot     = next_slot_++;
    const auto result_slot = next_slot_++;
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(object_slot, object_type));
    current_fn_->blocks[current_block_].insts.push(emitSlotStore(object_slot, object));
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(
        lo_slot,
        types_.kindOf(bound_type) == types::TypeKind::Int ? bound_type : types::kErrorType));
    current_fn_->blocks[current_block_].insts.push(emitSlotStore(lo_slot, lo));
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(
        hi_slot,
        types_.kindOf(bound_type) == types::TypeKind::Int ? bound_type : types::kErrorType));
    current_fn_->blocks[current_block_].insts.push(emitSlotStore(hi_slot, hi));
    current_fn_->blocks[current_block_].insts.push(emitSlotAlloca(result_slot, type));

    const auto bound_load = emitSlotLoad(lo_slot, bound_type);
    const auto hi_load    = emitSlotLoad(hi_slot, bound_type);
    hir::HirLiteral zero;
    zero.type          = bound_type;
    zero.i             = 0;
    hir::HirExprId len = hir::kInvalidHirExpr;
    const auto *array  = std::get_if<types::TypeArray>(&types_.lookup(object_type));
    if (array != nullptr) {
        hir::HirLiteral len_lit;
        len_lit.type = bound_type;
        len_lit.i    = static_cast<int64_t>(array->count);
        len          = addExpr(std::move(len_lit));
    } else {
        const auto loaded = emitSlotLoad(object_slot, object_type);
        auto len_i64 =
            addExpr(hir::HirField{loaded, 1U, types_.internInt(types::IntWidth::I64), object_type});
        const auto len64 = types_.internInt(types::IntWidth::I64);
        if (types_.kindOf(bound_type) != types::TypeKind::Int) {
            len = hir::kInvalidHirExpr;
        } else if (bound_type != len64) {
            len = addExpr(hir::HirCast{len_i64, len64, bound_type});
        } else {
            len = len_i64;
        }
    }
    if (len == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;

    hir::HirBinary lo_ge_zero;
    lo_ge_zero.lhs           = bound_load;
    lo_ge_zero.rhs           = addExpr(std::move(zero));
    lo_ge_zero.op            = hir::HirBinaryOp::Ge;
    lo_ge_zero.type          = types::kBoolType;
    lo_ge_zero.operand_type  = bound_type;
    const auto lo_ge_zero_id = addExpr(std::move(lo_ge_zero));

    hir::HirBinary hi_le_len;
    hi_le_len.lhs           = hi_load;
    hi_le_len.rhs           = len;
    hi_le_len.op            = hir::HirBinaryOp::Le;
    hi_le_len.type          = types::kBoolType;
    hi_le_len.operand_type  = bound_type;
    const auto hi_le_len_id = addExpr(std::move(hi_le_len));

    hir::HirBinary lo_le_hi;
    lo_le_hi.lhs           = bound_load;
    lo_le_hi.rhs           = hi_load;
    lo_le_hi.op            = hir::HirBinaryOp::Le;
    lo_le_hi.type          = types::kBoolType;
    lo_le_hi.operand_type  = bound_type;
    const auto lo_le_hi_id = addExpr(std::move(lo_le_hi));

    hir::HirBinary first_and;
    first_and.lhs           = lo_ge_zero_id;
    first_and.rhs           = hi_le_len_id;
    first_and.op            = hir::HirBinaryOp::And;
    first_and.type          = types::kBoolType;
    first_and.operand_type  = types::kBoolType;
    const auto first_and_id = addExpr(std::move(first_and));

    hir::HirBinary all_ok;
    all_ok.lhs          = first_and_id;
    all_ok.rhs          = lo_le_hi_id;
    all_ok.op           = hir::HirBinaryOp::And;
    all_ok.type         = types::kBoolType;
    all_ok.operand_type = types::kBoolType;

    const auto some_block  = newBlock();
    const auto none_block  = newBlock();
    const auto merge_block = newBlock();
    hir::HirBranch branch;
    branch.cond       = addExpr(std::move(all_ok));
    branch.then_block = static_cast<hir::HirDeclId>(some_block);
    branch.else_block = static_cast<hir::HirDeclId>(none_block);
    setTerminator(addExpr(std::move(branch)));

    setCurrentBlock(some_block);
    current_fn_->blocks[some_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    {
        hir::HirMakeSlice ok_slice;
        ok_slice.object      = emitSlotLoad(object_slot, object_type);
        ok_slice.lo          = emitSlotLoad(lo_slot, bound_type);
        ok_slice.hi          = emitSlotLoad(hi_slot, bound_type);
        ok_slice.type        = slice_type;
        ok_slice.object_type = object_type;
        ok_slice.bound_type  = bound_type;
        ok_slice.is_array    = slice.is_array;
        ok_slice.is_pointer  = slice.is_pointer;
        ok_slice.checked     = false;
        hir::HirMakeSome some;
        some.type  = type;
        some.value = addExpr(std::move(ok_slice));
        current_fn_->blocks[some_block].insts.push(
            emitSlotStore(result_slot, addExpr(std::move(some))));
        emitJump(merge_block);
    }

    setCurrentBlock(none_block);
    current_fn_->blocks[none_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    {
        hir::HirMakeNone none;
        none.type = type;
        current_fn_->blocks[none_block].insts.push(
            emitSlotStore(result_slot, addExpr(std::move(none))));
        emitJump(merge_block);
    }

    setCurrentBlock(merge_block);
    current_fn_->blocks[merge_block].insts = memory::DynArray<hir::HirExprId>(arena_);
    return emitSlotLoad(result_slot, type);
}

} // namespace modern
} // namespace zith::sema

