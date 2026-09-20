#include "sema/hir-lower-modern.hpp"

#include "common/overloaded.hpp"
#include "diagnostics/error-codes.hpp"
#include "sema/hir-lower-utils.hpp"
#include "sema/op-mapping.hpp"
#include "support/int-literal.hpp"
#include "types/type-kind.hpp"

namespace zith::sema {
namespace modern {

hir::HirExprId HirLowerModern::lowerExpr(frontend::ExprId id) {
    if (!id || current_module_ == nullptr ||
        id.value > current_module_->frontend->expressions().size())
        return hir::kInvalidHirExpr;

    const auto &expr = current_module_->frontend->expressions()[id.value - 1U];
    const auto type  = typeOfExpr(id);
    switch (expr.kind) {
    case frontend::ExprKind::OwnershipCoerce:
        return expr.operands.empty() ? hir::kInvalidHirExpr : lowerExpr(expr.operands[0]);
    case frontend::ExprKind::Literal: {
        // Implicit `T -> opaque` coercions record the concrete source type but
        // sema also types the whole value expression as `opaque`. The HIR
        // literal must keep the concrete type so it can be spilled into the
        // erased payload before wrapping.
        types::TypeId literal_type = type;
        if (current_types_ != nullptr) {
            if (const auto *source = current_types_->opaqueSourceTypes.get(id.value)) {
                const sema::modern::TypeId source_sema =
                    current_instantiation_ != nullptr && current_instance_ != nullptr
                        ? current_instantiation_->substituteType(*source, current_instance_->args)
                        : *source;
                const types::TypeId lowered = lowerType(source_sema);
                if (lowered != types::kErrorType && lowered != types::kInvalidType)
                    literal_type = lowered;
            }
        }
        return lowerLiteral(expr, literal_type);
    }
    case frontend::ExprKind::Name:
        return lowerName(expr);
    case frontend::ExprKind::Unary:
        return lowerUnary(expr, type);
    case frontend::ExprKind::Binary:
        return lowerBinary(expr, type);
    case frontend::ExprKind::Pipe:
    case frontend::ExprKind::PipeDo:
        return lowerPipe(expr, type);
    case frontend::ExprKind::PipeCurrent:
        return lowerPipeCurrent(expr);
    case frontend::ExprKind::Call:
        return lowerCall(expr);
    case frontend::ExprKind::DockCall:
        // `dock` is a plain call expression whose result carries the machine
        // return type; sema already resolved the target state declaration.
        return lowerCall(expr);
    case frontend::ExprKind::Block:
        return lowerBlock(expr);
    case frontend::ExprKind::If:
        return lowerIf(expr, type);
    case frontend::ExprKind::When:
        return lowerWhen(expr, type);
    case frontend::ExprKind::WhenGuard:
        // `when` guards are lowered contextually by lowerWhenCondition, never
        // as standalone values.
        return hir::kInvalidHirExpr;
    case frontend::ExprKind::Range:
        // A literal range has a value type only in a `when` pattern or as the
        // RHS of `in`; both are lowered by their surrounding expression.
        return hir::kInvalidHirExpr;
    case frontend::ExprKind::While:
        return lowerWhile(expr);
    case frontend::ExprKind::For:
        return lowerFor(expr);
    case frontend::ExprKind::ForIn:
        return lowerForIn(expr);
    case frontend::ExprKind::Assign:
        return lowerAssign(expr, type);
    case frontend::ExprKind::OptionalProp:
        return lowerOptionalProp(expr, type);
    case frontend::ExprKind::Index:
        return lowerIndex(expr, type);
    case frontend::ExprKind::SliceRange:
        return lowerSliceRange(expr, type);
    case frontend::ExprKind::Field:
        return lowerField(expr, type);
    case frontend::ExprKind::Arrow:
        return lowerArrow(expr, type);
    case frontend::ExprKind::StructLiteral:
        return lowerStructLiteral(expr, type);
    case frontend::ExprKind::PackLiteral:
        return lowerPackLiteral(expr, type);
    case frontend::ExprKind::ArrayLiteral:
        return lowerArrayLiteral(expr, type);
    case frontend::ExprKind::Cast:
        return lowerCast(expr, type);
    case frontend::ExprKind::IsNull:
        return lowerIsNull(expr);
    case frontend::ExprKind::IsType:
        return lowerIsType(expr);
    case frontend::ExprKind::LayoutIntrinsic:
        return lowerLayoutIntrinsic(expr);
    case frontend::ExprKind::MacroCall: {
        // Transparent: delegate to expansion.
        if (expr.expansion)
            return lowerExpr(expr.expansion);
        return hir::kInvalidHirExpr;
    }
    case frontend::ExprKind::Return:
    case frontend::ExprKind::Placeholder:
    case frontend::ExprKind::Error:
        return hir::kInvalidHirExpr;
    }
    return hir::kInvalidHirExpr;
}

} // namespace modern
} // namespace zith::sema

