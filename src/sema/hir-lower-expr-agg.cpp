#include "sema/hir-lower-modern.hpp"

#include "common/overloaded.hpp"
#include "diagnostics/error-codes.hpp"
#include "sema/hir-lower-utils.hpp"
#include "sema/op-mapping.hpp"
#include "support/int-literal.hpp"
#include "types/type-kind.hpp"

namespace zith::sema {
namespace modern {

memory::Optional<int64_t> HirLowerModern::enumVariantValue(frontend::ExprId operand,
                                                           std::string_view variant) {
    if (!operand || current_module_ == nullptr || current_module_->frontend == nullptr ||
        operand.value > current_module_->frontend->expressions().size()) {
        return {};
    }
    const auto &op = current_module_->frontend->expressions()[operand.value - 1U];
    if (op.kind != frontend::ExprKind::Name)
        return {};

    // Imported enums resolve through the sema type table even when the
    // frontend declaration belongs to another module. Prefer that table here
    // so `E.Ok` still produces a constant when `E` is not declared in the
    // current frontend.
    const auto sema_enum_type = sema_.typeTable().stripQualifiers(semaTypeOfExpr(operand));
    const auto *et            = sema_.typeTable().enum_type(sema_enum_type);
    if (et != nullptr) {
        for (size_t i = 0; i < et->variant_names.size(); ++i) {
            if (et->variant_names[i] == variant)
                return et->discriminants[i];
        }
    }

    const auto *resolved = findResolvedExpr(operand);
    if (resolved == nullptr || !resolved->declaration)
        return {};
    const auto *decl = findDecl(*current_module_, resolved->declaration);
    if (decl == nullptr || decl->kind != frontend::DeclKind::Enum)
        return {};
    // Enum variants are constants, but generic enum templates lower through a
    // concrete instance. Resolve the discriminant from the expression's actual
    // sema type (`Status<i32>`) instead of the template declaration.
    const auto sema_enum_type2 = sema_.typeTable().stripQualifiers(semaTypeOfExpr(operand));
    const auto *et2            = sema_.typeTable().enum_type(sema_enum_type2);
    if (et2 == nullptr)
        return {};
    for (size_t i = 0; i < et2->variant_names.size(); ++i) {
        if (et2->variant_names[i] == variant)
            return et2->discriminants[i];
    }
    return {};
}

hir::HirExprId HirLowerModern::lowerField(const frontend::Expression &expr,
                                          const types::TypeId type) {
    if (expr.operands.empty())
        return hir::kInvalidHirExpr;
    if (current_types_ != nullptr) {
        if (const auto *base = current_types_->traitQualifiedReceiverBase.get(expr.id.value))
            return lowerExpr(frontend::ExprId{*base});
    }
    // `console.println` where `console` is an import alias: the field expression resolves
    // to the imported symbol, so emit the same HirVar a plain name would produce and never
    // lower the alias base (which would fail to resolve 'console').
    if (const auto *resolved = findResolvedExpr(expr.id);
        resolved != nullptr && resolved->kind == session::ResolutionKind::Import) {
        const frontend::Declaration *decl = resolvedFunctionDecl(*resolved);
        if (decl != nullptr) {
            hir::HirVar var;
            var.name    = interner_.intern(decl->name);
            var.version = 0;
            return addExpr(std::move(var));
        }
        if (resolved->foreignFunction != nullptr) {
            hir::HirVar var;
            var.name    = interner_.intern(resolved->foreignFunction->linkageName);
            var.version = 0;
            return addExpr(std::move(var));
        }
        return hir::kInvalidHirExpr;
    }
    // `std.io.console.println(...)`, `std.counter.Counter`-style chains, and
    // other multi-segment module paths have intermediate Field nodes that
    // resolve as ModuleAlias. Those nodes carry no value; the final member is
    // the Import binding above, so intervening aliases must not lower.
    if (const auto *resolved = findResolvedExpr(expr.id);
        resolved != nullptr && resolved->kind == session::ResolutionKind::ModuleAlias) {
        const frontend::Expression *chain = &expr;
        while (chain->kind == frontend::ExprKind::Field && !chain->operands.empty()) {
            const auto &base =
                current_module_->frontend->expressions()[chain->operands[0].value - 1U];
            if (const auto *base_resolved = findResolvedExpr(chain->operands[0]);
                base_resolved != nullptr &&
                base_resolved->kind == session::ResolutionKind::Import) {
                const frontend::Declaration *decl = resolvedFunctionDecl(*base_resolved);
                if (decl != nullptr) {
                    hir::HirVar var;
                    var.name    = interner_.intern(decl->name);
                    var.version = 0;
                    return addExpr(std::move(var));
                }
                return hir::kInvalidHirExpr;
            }
            chain = &base;
        }
    }
    // `Color.Green` resolves to an enum variant constant, not a struct field read.
    if (const auto variant = enumVariantValue(expr.operands[0], expr.text))
        return addExpr(hir::HirEnumValue{*variant, type});
    auto object      = lowerExpr(expr.operands[0]);
    auto object_type = typeOfExpr(expr.operands[0]);
    if (object == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;
    // Resolve the sema struct type to find the field index by name
    auto sema_type = sema_.typeTable().stripQualifiers(semaTypeOfExpr(expr.operands[0]));
    // `self.field` / `p.field` auto-derefs an implicit receiver or borrow
    // parameter in sema; mirror it here so the field access still lowers to a
    // loaded value.
    if (const auto *sem_ptr = sema_.typeTable().pointer(sema_type)) {
        const auto struct_type = lowerType(sema_.typeTable().stripQualifiers(sem_ptr->pointee));
        object      = addExpr(hir::HirUnary{hir::HirUnaryOp::Deref, object, struct_type});
        object_type = struct_type;
        sema_type   = sema_.typeTable().stripQualifiers(sem_ptr->pointee);
    }
    const int idx = sema_.typeTable().fieldIndex(sema_type, expr.text);
    if (idx >= 0)
        return addExpr(hir::HirField{object, static_cast<uint32_t>(idx), type, object_type});
    if (sema_.typeTable().kindOf(sema_type) == sema::modern::TypeKind::Pack) {
        const auto *pack = sema_.typeTable().pack(sema_type);
        if (pack != nullptr) {
            for (size_t index = 0; index < pack->names.size(); ++index) {
                if (pack->names[index] != expr.text)
                    continue;
                return addExpr(
                    hir::HirField{object, static_cast<uint32_t>(index), type, object_type});
            }
        }
    }
    if (current_module_ != nullptr &&
        sema_.typeTable().kindOf(sema_type) == sema::modern::TypeKind::GenericParam) {
        const auto *module_sema = sema_.findModuleSema(current_module_->key);
        if (module_sema != nullptr) {
            for (const TypeId bound : module_sema->boundsForGenericParam(sema_type)) {
                if (!module_sema->isInterfaceType(bound))
                    continue;
                const auto *trait_ty = sema_.typeTable().trait(module_sema->resolve(bound));
                if (trait_ty == nullptr)
                    continue;
                const auto *iface =
                    module_sema->findDeclNamed(trait_ty->name, frontend::DeclKind::Interface);
                if (iface == nullptr)
                    continue;
                for (size_t field = 0; field < iface->parameters.size(); ++field) {
                    if (iface->parameters[field].name != expr.text)
                        continue;
                    return addExpr(
                        hir::HirField{object, static_cast<uint32_t>(field), type, object_type});
                }
            }
        }
    }
    return hir::kInvalidHirExpr;
}

hir::HirExprId HirLowerModern::lowerArrow(const frontend::Expression &expr,
                                          const types::TypeId type) {
    if (expr.operands.empty())
        return hir::kInvalidHirExpr;
    const auto ptr = lowerExpr(expr.operands[0]);
    if (ptr == hir::kInvalidHirExpr)
        return hir::kInvalidHirExpr;
    // Deref the pointer first
    auto sema_ptr_type = sema_.typeTable().stripQualifiers(semaTypeOfExpr(expr.operands[0]));
    // Match sema: `?*T` behaves as `*T` here because None is represented as nullptr.
    if (const auto *opt = sema_.typeTable().optional(sema_ptr_type))
        sema_ptr_type = sema_.typeTable().stripQualifiers(opt->inner);
    const auto *pt = sema_.typeTable().pointer(sema_ptr_type);
    if (pt == nullptr)
        return hir::kInvalidHirExpr;
    const auto sema_struct = sema_.typeTable().stripQualifiers(pt->pointee);
    const auto struct_type = lowerType(sema_struct);
    const auto deref       = addExpr(hir::HirUnary{hir::HirUnaryOp::Deref, ptr, struct_type});
    const int idx          = sema_.typeTable().fieldIndex(sema_struct, expr.text);
    if (idx < 0)
        return hir::kInvalidHirExpr;
    return addExpr(hir::HirField{deref, static_cast<uint32_t>(idx), type, struct_type});
}

hir::HirExprId HirLowerModern::lowerStructLiteral(const frontend::Expression &expr,
                                                  const types::TypeId type) {
    if (types_.kindOf(type) == types::TypeKind::Union) {
        if (expr.operands.size() != 1U)
            return hir::kInvalidHirExpr;
        const auto value = lowerExpr(expr.operands[0]);
        if (value == hir::kInvalidHirExpr)
            return hir::kInvalidHirExpr;
        const auto from = typeOfExpr(expr.operands[0]);
        hir::HirUnionCast cast;
        cast.value        = value;
        cast.from         = from;
        cast.to           = type;
        cast.member_index = taggedMemberIndex(type, from);
        cast.checked      = false;
        return addExpr(std::move(cast));
    }
    hir::HirStructLiteral lit(arena_);
    lit.type = type;
    const size_t field_count =
        types_.kindOf(type) == types::TypeKind::Struct ? types_.fieldCount(type) : 0U;
    // Values are emitted in declaration order, not in the order the literal was written.
    std::vector<hir::HirExprId> ordered(field_count == 0U ? expr.operands.size() : field_count,
                                        hir::kInvalidHirExpr);
    for (size_t i = 0; i < expr.operands.size(); ++i) {
        auto value = lowerExpr(expr.operands[i]);
        if (value == hir::kInvalidHirExpr)
            continue;
        size_t slot_index = i;
        if (field_count != 0U && i < expr.field_names.size()) {
            slot_index = types_.fieldIndex(type, expr.field_names[i]);
            if (slot_index >= field_count)
                continue;
        }
        if (field_count != 0U) {
            // A bare `T` value assigned to a `?T` field must be wrapped in Some.
            const auto field_type = types_.getField(type, slot_index).type;
            const auto value_type = typeOfExpr(expr.operands[i]);
            if (types_.kindOf(field_type) == types::TypeKind::Optional &&
                types_.kindOf(value_type) != types::TypeKind::Optional) {
                value = lowerCoerceToOptional(field_type, value);
            }
            value = lowerCoerceToTarget(field_type, expr.operands[i], value);
        }
        if (slot_index < ordered.size())
            ordered[slot_index] = value;
    }
    // Fill slots left empty by a `_` placeholder or an omitted field with the
    // struct declaration's default; slots without a default stay zero-initialized.
    if (field_count != 0U) {
        for (size_t slot_index = 0; slot_index < field_count; ++slot_index) {
            if (ordered[slot_index] != hir::kInvalidHirExpr)
                continue;
            const auto default_id = lowerFieldDefault(expr.text, slot_index);
            if (!default_id)
                continue;
            const auto default_value = lowerExpr(default_id);
            if (default_value == hir::kInvalidHirExpr)
                continue;
            const auto field_type = types_.getField(type, slot_index).type;
            ordered[slot_index] =
                types_.kindOf(field_type) == types::TypeKind::Optional &&
                        types_.kindOf(typeOfExpr(default_id)) != types::TypeKind::Optional
                    ? lowerCoerceToOptional(field_type, default_value)
                    : lowerCoerceToTarget(field_type, default_id, default_value);
        }
    }
    // Keep every slot (missing ones are zero at codegen); the array is index-aligned.
    for (const auto value : ordered)
        lit.values.push(value);
    return addExpr(std::move(lit));
}

hir::HirExprId HirLowerModern::lowerPackLiteral(const frontend::Expression &expr,
                                                const types::TypeId type) {
    hir::HirStructLiteral lit(arena_);
    lit.type = type;
    for (const auto operand : expr.operands) {
        const auto value = lowerExpr(operand);
        if (value == hir::kInvalidHirExpr)
            return hir::kInvalidHirExpr;
        lit.values.push(value);
    }
    return addExpr(std::move(lit));
}

hir::HirExprId HirLowerModern::lowerArrayLiteral(const frontend::Expression &expr,
                                                 const types::TypeId type) {
    hir::HirArrayLiteral lit(arena_);
    lit.type                 = type;
    const auto *target_array = std::get_if<types::TypeArray>(&types_.lookup(type));
    for (size_t index = 0; index < expr.operands.size(); ++index) {
        const frontend::ExprId operand = expr.operands[index];
        auto value                     = lowerExpr(operand);
        if (value == hir::kInvalidHirExpr)
            return hir::kInvalidHirExpr;
        if (target_array != nullptr)
            value = lowerCoerceToTarget(target_array->elem, operand, value);
        lit.elements.push(value);
    }
    return addExpr(std::move(lit));
}

frontend::ExprId HirLowerModern::lowerFieldDefault(std::string_view struct_name,
                                                   size_t field_index) const noexcept {
    if (current_module_ == nullptr || current_module_->frontend == nullptr)
        return {};
    const auto &decls = current_module_->frontend->declarations();
    for (const auto &decl : decls) {
        if (decl.kind != frontend::DeclKind::Struct || decl.name != struct_name)
            continue;
        if (field_index < decl.parameters.size())
            return decl.parameters[field_index].defaultValue;
        break;
    }
    return {};
}

} // namespace modern
} // namespace zith::sema

