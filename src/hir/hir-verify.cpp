#include "hir-verify.hpp"

#include "diagnostics/error-codes.hpp"
#include "types/type-intern.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace zith::hir {

HirVerifier::HirVerifier(const HirModule &module, const types::TypeIntern &types,
                         diagnostics::DiagnosticEngine &diags)
    : module_(module), types_(types), diags_(diags) {}

bool HirVerifier::verify() {
    bool valid        = true;
    const auto report = [&](std::string message) {
        valid = false;
        diags_.reportError(diagnostics::err::InvalidIR, std::move(message), {});
    };
    const auto checkExpr = [&](const HirExprId id, const char *role,
                               const bool allow_invalid = false) {
        if (id == kInvalidHirExpr && allow_invalid)
            return;
        if (id == kInvalidHirExpr || id >= module_.exprCount())
            report(std::string("HIR ") + role + " references an invalid expression");
    };
    const auto checkType = [&](const types::TypeId id, const char *role,
                               const bool allow_invalid = false) {
        if (id == types::kInvalidType && allow_invalid)
            return;
        if (id == types::kInvalidType || id >= types_.count())
            report(std::string("HIR ") + role + " references an invalid type");
    };
    const auto checkCall = [&](const HirCall &call) {
        checkExpr(call.callee, "call callee", true);
        if (call.resolved_fn != kInvalidHirFunction && call.resolved_fn >= module_.getFnCount())
            report("HIR call references an invalid function");
        if (call.args.size() != call.argument_types.size())
            report("HIR call argument and argument-type counts differ");
        for (const auto arg : call.args)
            checkExpr(arg, "call argument");
        for (const auto type : call.argument_types)
            checkType(type, "call argument");
        checkType(call.fn_type, "call function signature", true);
    };
    const auto checkSafety = [&](const HirExprId id, const HirSafetyMode mode,
                                 const HirSafetyProofKind expected_kind, const HirExprId operand_a,
                                 const HirExprId operand_b, const HirExprId operand_c,
                                 const bool proof_allowed = true) {
        if (static_cast<uint8_t>(mode) > static_cast<uint8_t>(HirSafetyMode::Raw))
            report("HIR expression has an unknown safety mode");
        const auto *proof = module_.attrs().trySafetyProof(id);
        if (proof == nullptr || !proof->present)
            return;
        if (mode != HirSafetyMode::RequiredCheck)
            report("HIR safety proof is attached to an operation without a required check");
        if (!proof_allowed || proof->kind != expected_kind)
            report("HIR safety proof kind does not match its operation");
        if (proof->operand_a != operand_a || proof->operand_b != operand_b ||
            proof->operand_c != operand_c)
            report("HIR safety proof operands do not match its operation");
    };
    const auto optionalSlot = [&](const HirExprId id) -> std::optional<HirSlotId> {
        if (id == kInvalidHirExpr || id >= module_.exprCount())
            return std::nullopt;
        const auto *address = std::get_if<HirSlotAddr>(&module_.getExpr(id));
        if (address == nullptr || address->slot == kInvalidHirSlot)
            return std::nullopt;
        return address->slot;
    };
    const auto slotOfValue = [&](const HirExprId id) -> std::optional<HirSlotId> {
        if (id == kInvalidHirExpr || id >= module_.exprCount())
            return std::nullopt;
        const auto &expr = module_.getExpr(id);
        if (const auto *address = std::get_if<HirSlotAddr>(&expr);
            address != nullptr && address->slot != kInvalidHirSlot)
            return address->slot;
        if (const auto *load = std::get_if<HirSlotLoad>(&expr);
            load != nullptr && load->slot != kInvalidHirSlot)
            return load->slot;
        return std::nullopt;
    };
    const auto sameValue = [&](const HirExprId lhs, const HirExprId rhs) {
        if (lhs == rhs)
            return true;
        const auto lhs_slot = slotOfValue(lhs);
        const auto rhs_slot = slotOfValue(rhs);
        return lhs_slot.has_value() && rhs_slot.has_value() && lhs_slot == rhs_slot;
    };
    const std::function<std::optional<bool>(HirExprId, HirSlotId)> presentWhenTrue =
        [&](const HirExprId id, const HirSlotId slot) -> std::optional<bool> {
        if (id == kInvalidHirExpr || id >= module_.exprCount())
            return std::nullopt;
        const auto &condition = module_.getExpr(id);
        if (const auto *field = std::get_if<HirField>(&condition)) {
            const auto field_slot = optionalSlot(field->object);
            if (field->index == 1U && field_slot == slot && field->object_type < types_.count() &&
                types_.kindOf(field->object_type) == types::TypeKind::Optional)
                return true;
        }
        if (const auto *unary = std::get_if<HirUnary>(&condition);
            unary != nullptr && unary->op == HirUnaryOp::Not) {
            const auto nested = presentWhenTrue(unary->operand, slot);
            if (nested.has_value())
                return !*nested;
        }
        return std::nullopt;
    };

    for (HirExprId id = 0; id < module_.exprCount(); ++id) {
        const auto &expr = module_.getExpr(id);
        visitExpr(expr, [&](const auto &node) {
            using Node = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<Node, HirLiteral>) {
                checkType(node.type, "literal");
            } else if constexpr (std::is_same_v<Node, HirBinary>) {
                checkExpr(node.lhs, "binary left operand");
                checkExpr(node.rhs, "binary right operand");
                checkType(node.type, "binary result");
                checkType(node.operand_type, "binary operand", true);
            } else if constexpr (std::is_same_v<Node, HirUnary>) {
                checkExpr(node.operand, "unary operand");
                checkType(node.type, "unary result");
            } else if constexpr (std::is_same_v<Node, HirLet>) {
                checkExpr(node.init, "let initializer", true);
                checkType(node.type, "let");
            } else if constexpr (std::is_same_v<Node, HirCall>) {
                checkCall(node);
            } else if constexpr (std::is_same_v<Node, HirRet>) {
                checkExpr(node.value, "return value", true);
            } else if constexpr (std::is_same_v<Node, HirBranch>) {
                checkExpr(node.cond, "branch condition");
            } else if constexpr (std::is_same_v<Node, HirPhi>) {
                for (const auto incoming : node.incoming)
                    checkExpr(incoming, "phi input");
            } else if constexpr (std::is_same_v<Node, HirAssign>) {
                checkExpr(node.target, "assignment target");
                checkExpr(node.value, "assignment value");
            } else if constexpr (std::is_same_v<Node, HirIndex>) {
                checkExpr(node.object, "index object");
                checkExpr(node.index, "index operand");
                checkType(node.type, "index result");
                checkType(node.obj_type, "index object");
                checkSafety(id, node.safety, HirSafetyProofKind::BoundsValid, node.object,
                            node.index, kInvalidHirExpr);
            } else if constexpr (std::is_same_v<Node, HirField>) {
                checkExpr(node.object, "field object");
                checkType(node.type, "field result");
                checkType(node.object_type, "field object");
                const bool optional_payload =
                    node.object_type != types::kInvalidType && node.object_type < types_.count() &&
                    types_.kindOf(node.object_type) == types::TypeKind::Optional;
                checkSafety(id, node.safety, HirSafetyProofKind::OptionalPresent, node.object,
                            kInvalidHirExpr, kInvalidHirExpr, optional_payload);
            } else if constexpr (std::is_same_v<Node, HirStructLiteral>) {
                checkType(node.type, "struct literal");
                for (const auto value : node.values)
                    checkExpr(value, "struct literal field");
            } else if constexpr (std::is_same_v<Node, HirArrayLiteral>) {
                checkType(node.type, "array literal");
                for (const auto value : node.elements)
                    checkExpr(value, "array literal element");
            } else if constexpr (std::is_same_v<Node, HirEnumValue>) {
                checkType(node.type, "enum value");
            } else if constexpr (std::is_same_v<Node, HirSlotAlloca>) {
                checkType(node.type, "slot allocation");
            } else if constexpr (std::is_same_v<Node, HirSlotStore>) {
                checkExpr(node.value, "slot store value");
            } else if constexpr (std::is_same_v<Node, HirSlotLoad> ||
                                 std::is_same_v<Node, HirSlotAddr>) {
                checkType(node.type, "slot result");
            } else if constexpr (std::is_same_v<Node, HirMakeNone>) {
                checkType(node.type, "none value");
            } else if constexpr (std::is_same_v<Node, HirCast>) {
                checkExpr(node.value, "cast value");
                checkType(node.from, "cast source");
                checkType(node.to, "cast target");
            } else if constexpr (std::is_same_v<Node, HirMakeSome>) {
                checkExpr(node.value, "some payload");
                checkType(node.type, "some value");
            } else if constexpr (std::is_same_v<Node, HirMakeSlice>) {
                checkExpr(node.object, "slice object");
                checkExpr(node.lo, "slice lower bound");
                checkExpr(node.hi, "slice upper bound");
                checkType(node.type, "slice result");
                checkType(node.object_type, "slice object");
                checkType(node.bound_type, "slice bound");
                checkSafety(id, node.safety, HirSafetyProofKind::BoundsValid, node.object, node.lo,
                            node.hi);
            } else if constexpr (std::is_same_v<Node, HirUnionCast>) {
                checkExpr(node.value, "union cast value");
                checkType(node.from, "union cast source");
                checkType(node.to, "union cast target");
                checkSafety(id, node.safety, HirSafetyProofKind::UnionMemberMatches, node.value,
                            kInvalidHirExpr, kInvalidHirExpr);
            } else if constexpr (std::is_same_v<Node, HirUnionCheck>) {
                checkExpr(node.value, "union check value");
                checkType(node.union_type, "union check");
            } else if constexpr (std::is_same_v<Node, HirLayoutIntrinsic>) {
                checkType(node.type, "layout intrinsic");
                checkExpr(node.operand, "layout operand", true);
                checkType(node.operand_type, "layout operand", true);
            } else if constexpr (std::is_same_v<Node, HirStateTailCall>) {
                checkCall(node.call);
            } else if constexpr (std::is_same_v<Node, HirCleanup>) {
                for (const auto cleanup_expr : node.exprs)
                    checkExpr(cleanup_expr, "cleanup");
            } else if constexpr (std::is_same_v<Node, HirGlobalConstLoad> ||
                                 std::is_same_v<Node, HirExternalGlobalAddr>) {
                checkType(node.type, "global");
            } else if constexpr (std::is_same_v<Node, HirMakeDyn>) {
                checkExpr(node.value, "dyn value");
                checkType(node.source_type, "dyn source");
                checkType(node.dyn_type, "dyn target");
            } else if constexpr (std::is_same_v<Node, HirDynCall>) {
                checkExpr(node.receiver, "dyn receiver");
                checkType(node.result_type, "dyn call result");
                checkType(node.fn_type, "dyn call signature");
                for (const auto arg : node.args)
                    checkExpr(arg, "dyn call argument");
                for (const auto type : node.arg_types)
                    checkType(type, "dyn call argument");
                if (node.args.size() != node.arg_types.size())
                    report("HIR dyn-call argument and argument-type counts differ");
            } else if constexpr (std::is_same_v<Node, HirMakeOpaque>) {
                checkExpr(node.value, "opaque value");
                checkType(node.source_type, "opaque source");
                checkType(node.opaque_type, "opaque target");
            } else if constexpr (std::is_same_v<Node, HirOpaqueCast>) {
                checkExpr(node.value, "opaque cast value");
                checkType(node.from, "opaque cast source");
                checkType(node.to, "opaque cast target");
                checkType(node.opaque_type, "opaque cast representation");
                checkType(node.result_type, "opaque cast result");
                checkSafety(id, node.safety, HirSafetyProofKind::OpaqueTypeMatches, node.value,
                            kInvalidHirExpr, kInvalidHirExpr);
            } else if constexpr (std::is_same_v<Node, HirOpaqueCheck>) {
                checkExpr(node.value, "opaque check value");
                checkType(node.opaque_type, "opaque check");
            } else if constexpr (std::is_same_v<Node, HirCanonicalType>) {
                checkType(node.type, "canonical type");
            } else if constexpr (std::is_same_v<Node, HirPipe>) {
                checkExpr(node.stage, "pipe stage");
                checkType(node.source_type, "pipe source");
                checkType(node.type, "pipe result");
            } else if constexpr (std::is_same_v<Node, HirPipeCurrent>) {
                checkType(node.type, "pipe current value");
            }
        });
        if (const auto *attrs = module_.attrs().tryCall(id);
            attrs != nullptr && (attrs->hasResidualFacts() || !attrs->args.empty())) {
            if (std::get_if<HirCall>(&expr) == nullptr)
                report("HIR call attributes are attached to a non-call expression");
        }
        if (const auto *proof = module_.attrs().trySafetyProof(id);
            proof != nullptr && proof->present) {
            const bool supported = std::get_if<HirIndex>(&expr) != nullptr ||
                                   std::get_if<HirField>(&expr) != nullptr ||
                                   std::get_if<HirMakeSlice>(&expr) != nullptr ||
                                   std::get_if<HirUnionCast>(&expr) != nullptr ||
                                   std::get_if<HirOpaqueCast>(&expr) != nullptr;
            if (!supported)
                report("HIR safety proof is attached to an unsupported expression kind");
        }
    }

    std::unordered_set<HirSlotId> module_slots;
    for (size_t fn_index = 0; fn_index < module_.getFnCount(); ++fn_index) {
        const auto &fn = module_.getFn(fn_index);
        checkType(fn.return_type, "function return");
        checkType(fn.machineReturnType, "state machine return", true);
        if (fn.hasBody != !fn.blocks.empty())
            report("HIR function body classification does not match its blocks");
        for (const auto param : fn.params)
            checkType(param, "function parameter");
        if (!fn.param_slots.empty() && fn.param_slots.size() != fn.params.size())
            report("HIR function parameter and slot counts differ");

        const size_t block_count = fn.blocks.size();
        std::vector<std::vector<size_t>> predecessors(block_count);
        for (size_t source = 0; source < block_count; ++source) {
            const HirExprId terminator = fn.blocks[source].terminator;
            if (terminator == kInvalidHirExpr || terminator >= module_.exprCount())
                continue;
            const auto &term = module_.getExpr(terminator);
            if (const auto *branch = std::get_if<HirBranch>(&term)) {
                if (branch->then_block < block_count)
                    predecessors[branch->then_block].push_back(source);
                if (branch->else_block < block_count)
                    predecessors[branch->else_block].push_back(source);
            } else if (const auto *jump = std::get_if<HirJump>(&term);
                       jump != nullptr && jump->target < block_count) {
                predecessors[jump->target].push_back(source);
            }
        }

        std::vector<bool> reachable(block_count, false);
        if (block_count != 0U) {
            std::vector<size_t> pending{0U};
            reachable[0] = true;
            for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
                const size_t source        = pending[cursor];
                const HirExprId terminator = fn.blocks[source].terminator;
                if (terminator == kInvalidHirExpr || terminator >= module_.exprCount())
                    continue;
                const auto &term       = module_.getExpr(terminator);
                const auto visitTarget = [&](const HirBlockId target) {
                    if (target < block_count && !reachable[target]) {
                        reachable[target] = true;
                        pending.push_back(target);
                    }
                };
                if (const auto *branch = std::get_if<HirBranch>(&term)) {
                    visitTarget(branch->then_block);
                    visitTarget(branch->else_block);
                } else if (const auto *jump = std::get_if<HirJump>(&term)) {
                    visitTarget(jump->target);
                }
            }
        }

        std::vector<std::vector<bool>> dominators(block_count,
                                                  std::vector<bool>(block_count, false));
        for (size_t block = 0; block < block_count; ++block) {
            if (!reachable[block]) {
                dominators[block][block] = true;
            } else if (block == 0U) {
                dominators[block][block] = true;
            } else {
                for (size_t candidate = 0; candidate < block_count; ++candidate)
                    dominators[block][candidate] = reachable[candidate];
            }
        }
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t block = 1; block < block_count; ++block) {
                if (!reachable[block])
                    continue;
                std::vector<bool> next(block_count, true);
                bool has_reachable_predecessor = false;
                for (const size_t predecessor : predecessors[block]) {
                    if (!reachable[predecessor])
                        continue;
                    has_reachable_predecessor = true;
                    for (size_t candidate = 0; candidate < block_count; ++candidate)
                        next[candidate] = next[candidate] && dominators[predecessor][candidate];
                }
                if (!has_reachable_predecessor)
                    std::fill(next.begin(), next.end(), false);
                next[block] = true;
                if (next != dominators[block]) {
                    dominators[block] = std::move(next);
                    changed           = true;
                }
            }
        }
        const auto selectedEdgeDominates = [&](const size_t use_block, const size_t guard_block,
                                               const bool selected_then) {
            if (use_block >= block_count || guard_block >= block_count || !reachable[use_block] ||
                !reachable[guard_block] || !dominators[use_block][guard_block])
                return false;

            std::vector<bool> visited(block_count, false);
            std::vector<size_t> pending{0U};
            visited[0] = true;
            for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
                const size_t source = pending[cursor];
                if (source == use_block)
                    return false;
                const HirExprId terminator = fn.blocks[source].terminator;
                if (terminator == kInvalidHirExpr || terminator >= module_.exprCount())
                    continue;
                const auto visit = [&](const HirBlockId target) {
                    if (target < block_count && reachable[target] && !visited[target]) {
                        visited[target] = true;
                        pending.push_back(target);
                    }
                };
                const auto &term = module_.getExpr(terminator);
                if (const auto *branch = std::get_if<HirBranch>(&term)) {
                    if (source == guard_block) {
                        if (selected_then)
                            visit(branch->else_block);
                        else
                            visit(branch->then_block);
                    } else {
                        visit(branch->then_block);
                        visit(branch->else_block);
                    }
                } else if (const auto *jump = std::get_if<HirJump>(&term)) {
                    visit(jump->target);
                }
            }
            return true;
        };
        const auto guardPolarity = [&](auto &&self, const HirExprId condition,
                                       const auto &matches) -> std::optional<bool> {
            if (condition == kInvalidHirExpr || condition >= module_.exprCount())
                return std::nullopt;
            if (const auto direct = matches(condition); direct.has_value())
                return direct;
            const auto *unary = std::get_if<HirUnary>(&module_.getExpr(condition));
            if (unary == nullptr || unary->op != HirUnaryOp::Not)
                return std::nullopt;
            const auto nested = self(self, unary->operand, matches);
            return nested.has_value() ? std::optional<bool>{!*nested} : std::nullopt;
        };
        const auto hasDominatingGuard = [&](const size_t use_block, const auto &matches) {
            if (use_block >= block_count || !reachable[use_block])
                return false;
            for (size_t guard_block = 0; guard_block < block_count; ++guard_block) {
                if (!reachable[guard_block] || !dominators[use_block][guard_block])
                    continue;
                const HirExprId terminator = fn.blocks[guard_block].terminator;
                if (terminator == kInvalidHirExpr || terminator >= module_.exprCount())
                    continue;
                const auto *branch = std::get_if<HirBranch>(&module_.getExpr(terminator));
                if (branch == nullptr || branch->then_block == branch->else_block)
                    continue;
                const auto satisfied_when_true =
                    guardPolarity(guardPolarity, branch->cond, matches);
                if (!satisfied_when_true.has_value())
                    continue;
                const bool selected_then = *satisfied_when_true;
                if (selectedEdgeDominates(use_block, guard_block, selected_then))
                    return true;
            }
            return false;
        };
        const auto isSlotLoad = [&](const HirExprId id, const HirSlotId slot) {
            if (id == kInvalidHirExpr || id >= module_.exprCount())
                return false;
            const auto *load = std::get_if<HirSlotLoad>(&module_.getExpr(id));
            return load != nullptr && load->slot == slot;
        };
        const auto isZero = [&](const HirExprId id) {
            if (id == kInvalidHirExpr || id >= module_.exprCount())
                return false;
            const auto *literal = std::get_if<HirLiteral>(&module_.getExpr(id));
            return literal != nullptr && literal->i == 0;
        };
        const auto isLengthOf = [&](const HirExprId id, const HirTypeId object_type,
                                    const HirSlotId object_slot) {
            if (object_type >= types_.count() || id == kInvalidHirExpr || id >= module_.exprCount())
                return false;
            if (const auto *array = std::get_if<types::TypeArray>(&types_.lookup(object_type))) {
                const auto *literal = std::get_if<HirLiteral>(&module_.getExpr(id));
                return literal != nullptr && literal->i >= 0 &&
                       static_cast<uint64_t>(literal->i) == array->count;
            }
            if (types_.kindOf(object_type) != types::TypeKind::Slice)
                return false;
            HirExprId length = id;
            for (size_t depth = 0; depth < module_.exprCount(); ++depth) {
                if (length >= module_.exprCount())
                    return false;
                if (const auto *cast = std::get_if<HirCast>(&module_.getExpr(length))) {
                    length = cast->value;
                    continue;
                }
                const auto *field = std::get_if<HirField>(&module_.getExpr(length));
                return field != nullptr && field->index == 1U &&
                       field->object_type == object_type && isSlotLoad(field->object, object_slot);
            }
            return false;
        };
        const auto binary = [&](const HirExprId id, const HirBinaryOp op) -> const HirBinary * {
            if (id == kInvalidHirExpr || id >= module_.exprCount())
                return nullptr;
            const auto *value = std::get_if<HirBinary>(&module_.getExpr(id));
            return value != nullptr && value->op == op ? value : nullptr;
        };
        const auto matchesIndexGuard = [&](const HirExprId condition, const HirIndex &index) {
            const auto object_slot = slotOfValue(index.object);
            const auto index_slot  = slotOfValue(index.index);
            if (!object_slot.has_value() || !index_slot.has_value())
                return std::optional<bool>{};
            const auto *all_ok = binary(condition, HirBinaryOp::And);
            const auto *lower  = all_ok != nullptr ? binary(all_ok->lhs, HirBinaryOp::Ge) : nullptr;
            const auto *upper  = all_ok != nullptr ? binary(all_ok->rhs, HirBinaryOp::Lt) : nullptr;
            if (lower == nullptr || upper == nullptr || !isSlotLoad(lower->lhs, *index_slot) ||
                !isZero(lower->rhs) || !isSlotLoad(upper->lhs, *index_slot) ||
                !isLengthOf(upper->rhs, index.obj_type, *object_slot))
                return std::optional<bool>{};
            return std::optional<bool>{true};
        };
        const auto matchesSliceGuard = [&](const HirExprId condition, const HirMakeSlice &slice) {
            const auto object_slot = slotOfValue(slice.object);
            const auto lo_slot     = slotOfValue(slice.lo);
            const auto hi_slot     = slotOfValue(slice.hi);
            if (!object_slot.has_value() || !lo_slot.has_value() || !hi_slot.has_value())
                return std::optional<bool>{};
            const auto *all_ok = binary(condition, HirBinaryOp::And);
            const auto *first = all_ok != nullptr ? binary(all_ok->lhs, HirBinaryOp::And) : nullptr;
            const auto *lo_nonnegative =
                first != nullptr ? binary(first->lhs, HirBinaryOp::Ge) : nullptr;
            const auto *hi_bounded =
                first != nullptr ? binary(first->rhs, HirBinaryOp::Le) : nullptr;
            const auto *ordered =
                all_ok != nullptr ? binary(all_ok->rhs, HirBinaryOp::Le) : nullptr;
            if (lo_nonnegative == nullptr || hi_bounded == nullptr || ordered == nullptr ||
                !isSlotLoad(lo_nonnegative->lhs, *lo_slot) || !isZero(lo_nonnegative->rhs) ||
                !isSlotLoad(hi_bounded->lhs, *hi_slot) ||
                !isLengthOf(hi_bounded->rhs, slice.object_type, *object_slot) ||
                !isSlotLoad(ordered->lhs, *lo_slot) || !isSlotLoad(ordered->rhs, *hi_slot))
                return std::optional<bool>{};
            return std::optional<bool>{true};
        };
        const auto matchesOptionalFieldGuard = [&](const HirExprId condition,
                                                   const HirField &field) {
            if (field.index != 0U || field.object_type >= types_.count() ||
                types_.kindOf(field.object_type) != types::TypeKind::Optional)
                return std::optional<bool>{};
            const auto slot = optionalSlot(field.object);
            return slot.has_value() ? presentWhenTrue(condition, *slot) : std::optional<bool>{};
        };
        const auto matchesOpaqueGuard = [&](const HirExprId condition, const HirOpaqueCast &cast) {
            if (condition == kInvalidHirExpr || condition >= module_.exprCount())
                return std::optional<bool>{};
            const auto *check = std::get_if<HirOpaqueCheck>(&module_.getExpr(condition));
            if (check == nullptr || check->opaque_type != cast.opaque_type ||
                check->type_id != cast.type_id || check->canonical_id.hi != cast.canonical_id.hi ||
                check->canonical_id.lo != cast.canonical_id.lo ||
                !sameValue(check->value, cast.value))
                return std::optional<bool>{};
            return std::optional<bool>{true};
        };
        const auto matchesUnionGuard = [&](const HirExprId condition, const HirUnionCast &cast) {
            if (condition == kInvalidHirExpr || condition >= module_.exprCount() ||
                cast.from >= types_.count() || types_.kindOf(cast.from) != types::TypeKind::Union)
                return std::optional<bool>{};
            const auto *check = std::get_if<HirUnionCheck>(&module_.getExpr(condition));
            if (check == nullptr || check->union_type != cast.from ||
                cast.member_index >= types_.getUnionDef(cast.from).members.size())
                return std::optional<bool>{};
            const auto *tag = check->value < module_.exprCount()
                                  ? std::get_if<HirField>(&module_.getExpr(check->value))
                                  : nullptr;
            if (tag == nullptr || tag->index != 1U || tag->object_type != cast.from ||
                !sameValue(tag->object, cast.value))
                return std::optional<bool>{};
            if (check->member_index == cast.member_index)
                return std::optional<bool>{true};
            const auto &members = types_.getUnionDef(cast.from).members;
            if (members.size() == 2U && check->member_index < members.size())
                return std::optional<bool>{false};
            return std::optional<bool>{};
        };
        const std::function<void(HirExprId, std::vector<HirExprId> &,
                                 std::unordered_set<HirExprId> &)>
            appendExprTree = [&](const HirExprId id, std::vector<HirExprId> &out,
                                 std::unordered_set<HirExprId> &seen) {
                if (id == kInvalidHirExpr || id >= module_.exprCount() || !seen.insert(id).second)
                    return;
                out.push_back(id);
                const auto append = [&](const HirExprId child) {
                    appendExprTree(child, out, seen);
                };
                visitExpr(module_.getExpr(id), [&](const auto &node) {
                    using Node = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<Node, HirBinary>) {
                        append(node.lhs);
                        append(node.rhs);
                    } else if constexpr (std::is_same_v<Node, HirUnary>) {
                        append(node.operand);
                    } else if constexpr (std::is_same_v<Node, HirLet>) {
                        append(node.init);
                    } else if constexpr (std::is_same_v<Node, HirMakeSome> ||
                                         std::is_same_v<Node, HirCast> ||
                                         std::is_same_v<Node, HirUnionCast> ||
                                         std::is_same_v<Node, HirUnionCheck> ||
                                         std::is_same_v<Node, HirMakeDyn> ||
                                         std::is_same_v<Node, HirMakeOpaque> ||
                                         std::is_same_v<Node, HirOpaqueCast> ||
                                         std::is_same_v<Node, HirOpaqueCheck>) {
                        append(node.value);
                    } else if constexpr (std::is_same_v<Node, HirCall>) {
                        append(node.callee);
                        for (const auto arg : node.args)
                            append(arg);
                    } else if constexpr (std::is_same_v<Node, HirRet>) {
                        append(node.value);
                    } else if constexpr (std::is_same_v<Node, HirBranch>) {
                        append(node.cond);
                    } else if constexpr (std::is_same_v<Node, HirPhi>) {
                        for (const auto incoming : node.incoming)
                            append(incoming);
                    } else if constexpr (std::is_same_v<Node, HirAssign>) {
                        append(node.target);
                        append(node.value);
                    } else if constexpr (std::is_same_v<Node, HirIndex>) {
                        append(node.object);
                        append(node.index);
                    } else if constexpr (std::is_same_v<Node, HirField>) {
                        append(node.object);
                    } else if constexpr (std::is_same_v<Node, HirStructLiteral>) {
                        for (const auto value : node.values)
                            append(value);
                    } else if constexpr (std::is_same_v<Node, HirArrayLiteral>) {
                        for (const auto value : node.elements)
                            append(value);
                    } else if constexpr (std::is_same_v<Node, HirSlotStore>) {
                        append(node.value);
                    } else if constexpr (std::is_same_v<Node, HirMakeSlice>) {
                        append(node.object);
                        append(node.lo);
                        append(node.hi);
                    } else if constexpr (std::is_same_v<Node, HirLayoutIntrinsic>) {
                        append(node.operand);
                    } else if constexpr (std::is_same_v<Node, HirStateTailCall>) {
                        append(node.call.callee);
                        for (const auto arg : node.call.args)
                            append(arg);
                    } else if constexpr (std::is_same_v<Node, HirCleanup>) {
                        for (const auto cleanup_expr : node.exprs)
                            append(cleanup_expr);
                    } else if constexpr (std::is_same_v<Node, HirDynCall>) {
                        append(node.receiver);
                        for (const auto arg : node.args)
                            append(arg);
                    } else if constexpr (std::is_same_v<Node, HirPipe>) {
                        append(node.stage);
                    }
                });
            };

        std::unordered_set<HirSlotId> defined_slots;
        std::unordered_set<HirSlotId> used_slots;
        for (const auto slot : fn.param_slots) {
            if (slot == kInvalidHirSlot)
                continue;
            else {
                defined_slots.insert(slot);
                module_slots.insert(slot);
            }
        }
        for (size_t block_index = 0; block_index < fn.blocks.size(); ++block_index) {
            const auto &block = fn.blocks[block_index];
            std::vector<HirExprId> block_exprs;
            block_exprs.reserve(block.insts.size() + 1U);
            std::unordered_set<HirExprId> seen_exprs;
            for (const auto inst : block.insts) {
                checkExpr(inst, "basic-block instruction");
                appendExprTree(inst, block_exprs, seen_exprs);
            }
            checkExpr(block.terminator, "basic-block terminator", true);
            appendExprTree(block.terminator, block_exprs, seen_exprs);

            for (const auto expr_id : block_exprs) {
                const auto &expr = module_.getExpr(expr_id);
                visitExpr(expr, [&](const auto &node) {
                    using Node = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<Node, HirField>) {
                        if (node.safety == HirSafetyMode::Guarded &&
                            !hasDominatingGuard(block_index, [&](const HirExprId condition) {
                                return matchesOptionalFieldGuard(condition, node);
                            }))
                            report("HIR guarded optional payload lacks its dominating true edge");
                    } else if constexpr (std::is_same_v<Node, HirIndex>) {
                        if (node.safety == HirSafetyMode::Guarded &&
                            !hasDominatingGuard(block_index, [&](const HirExprId condition) {
                                return matchesIndexGuard(condition, node);
                            }))
                            report("HIR guarded index lacks its dominating bounds-check edge");
                    } else if constexpr (std::is_same_v<Node, HirMakeSlice>) {
                        if (node.safety == HirSafetyMode::Guarded &&
                            !hasDominatingGuard(block_index, [&](const HirExprId condition) {
                                return matchesSliceGuard(condition, node);
                            }))
                            report("HIR guarded slice lacks its dominating bounds-check edge");
                    } else if constexpr (std::is_same_v<Node, HirUnionCast>) {
                        if (node.safety == HirSafetyMode::Guarded &&
                            !hasDominatingGuard(block_index, [&](const HirExprId condition) {
                                return matchesUnionGuard(condition, node);
                            }))
                            report("HIR guarded union cast lacks its matching member-check edge");
                    } else if constexpr (std::is_same_v<Node, HirOpaqueCast>) {
                        if (node.safety == HirSafetyMode::Guarded &&
                            !hasDominatingGuard(block_index, [&](const HirExprId condition) {
                                return matchesOpaqueGuard(condition, node);
                            }))
                            report("HIR guarded opaque cast lacks its matching type-check edge");
                    }
                });
                if (const auto *alloca = std::get_if<HirSlotAlloca>(&expr)) {
                    if (alloca->slot == kInvalidHirSlot)
                        report("HIR slot allocation has an invalid slot");
                    else {
                        defined_slots.insert(alloca->slot);
                        module_slots.insert(alloca->slot);
                    }
                } else if (const auto *store = std::get_if<HirSlotStore>(&expr)) {
                    used_slots.insert(store->slot);
                } else if (const auto *load = std::get_if<HirSlotLoad>(&expr)) {
                    used_slots.insert(load->slot);
                } else if (const auto *addr = std::get_if<HirSlotAddr>(&expr)) {
                    used_slots.insert(addr->slot);
                } else if (const auto *pipe = std::get_if<HirPipe>(&expr)) {
                    used_slots.insert(pipe->source_slot);
                } else if (const auto *current = std::get_if<HirPipeCurrent>(&expr)) {
                    used_slots.insert(current->slot);
                }

                if (const auto *branch = std::get_if<HirBranch>(&expr)) {
                    if (branch->then_block >= fn.blocks.size() ||
                        branch->else_block >= fn.blocks.size())
                        report("HIR branch target is outside its function");
                } else if (const auto *jump = std::get_if<HirJump>(&expr)) {
                    if (jump->target >= fn.blocks.size())
                        report("HIR jump target is outside its function");
                }
            }
        }
        for (const auto slot : used_slots) {
            if (slot == kInvalidHirSlot || !defined_slots.contains(slot))
                report("HIR slot use has no function-local allocation or parameter");
        }
    }

    for (size_t slot = 0; slot < module_.attrs().slotCount(); ++slot) {
        if (!module_slots.contains(static_cast<HirSlotId>(slot)))
            report("HIR slot attributes refer to an undefined slot");
    }
    if (module_.attrs().fnCount() > module_.getFnCount())
        report("HIR function attributes exceed the function count");
    if (module_.attrs().callCount() > module_.exprCount())
        report("HIR call attributes exceed the expression count");
    if (module_.attrs().safetyProofCount() > module_.exprCount())
        report("HIR safety proofs exceed the expression count");
    for (size_t expr_id = 0;
         expr_id < module_.attrs().safetyProofCount() && expr_id < module_.exprCount(); ++expr_id) {
        const auto *proof = module_.attrs().trySafetyProof(static_cast<HirExprId>(expr_id));
        if (proof != nullptr && proof->present &&
            static_cast<uint8_t>(proof->kind) >
                static_cast<uint8_t>(HirSafetyProofKind::OpaqueTypeMatches))
            report("HIR safety proof has an unknown proof kind");
    }

    for (size_t global_index = 0; global_index < module_.getGlobalConstCount(); ++global_index) {
        const auto &global = module_.getGlobalConst(global_index);
        checkType(global.type, "global constant");
        checkExpr(global.init, "global constant initializer");
    }
    for (size_t vtable_index = 0; vtable_index < module_.getVTableCount(); ++vtable_index) {
        const auto &vtable = module_.getVTable(vtable_index);
        for (const auto function_id : vtable.slots) {
            if (function_id == kInvalidHirFunction || function_id >= module_.getFnCount())
                report("HIR vtable references an invalid function");
        }
    }
    return valid;
}

} // namespace zith::hir
