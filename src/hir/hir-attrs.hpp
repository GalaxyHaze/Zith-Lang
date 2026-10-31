#pragma once

#include "hir/hir-expr.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "types/type-kind.hpp"

#include <cstdint>

namespace zith::hir {

/// Residual ownership fact attached to a slot/lowering object.
enum class HirOwnership : uint8_t {
    Default,
    Lend,
    View,
    Unique,
    Share,
    Belong,
};

/// State of a slot after the SRA proof boundary.
enum class HirConsumedState : uint8_t {
    Unknown,
    Consumed,
    NonConsumed,
};

/// Per-argument decision at a call site. The plan keeps these facts without
/// introducing ownership nodes into the expression graph.
enum class HirCallEscape : uint8_t {
    None,
    Borrow,
    Capture,
    Escape,
    Move,
};

struct HirSlotAttrs {
    HirOwnership ownership    = HirOwnership::Default;
    HirConsumedState consumed = HirConsumedState::Unknown;
    bool nonNull              = false;
    bool volatileSlot         = false;

    [[nodiscard]] constexpr bool hasResidualFacts() const noexcept {
        return ownership != HirOwnership::Default || consumed != HirConsumedState::Unknown ||
               nonNull || volatileSlot;
    }
};

struct HirCallArgAttr {
    HirCallEscape escape = HirCallEscape::None;

    [[nodiscard]] constexpr bool hasResidualFacts() const noexcept {
        return escape != HirCallEscape::None;
    }
};

struct HirCallAttrs {
    memory::DynArray<HirCallArgAttr> args;
    /// Escape facts correspond to semantic call arguments before lowering may
    /// coalesce a variadic tail into one HIR operand.
    /// When the proven return is the same node as an argument, this source
    /// argument index is kept so consumers can use the fact lazily.
    uint32_t returnsArg = ~0U;

    explicit HirCallAttrs(memory::Arena &arena) : args(arena) {}

    [[nodiscard]] bool hasResidualFacts() const noexcept {
        if (returnsArg != ~0U)
            return true;
        for (const auto &arg : args) {
            if (arg.hasResidualFacts())
                return true;
        }
        return false;
    }
};

struct HirFnAttrs {
    HirConsumedState returnConsumed = HirConsumedState::Unknown;
    bool nonNull                    = false;
    bool noAlias                    = false;
    bool readOnly                   = false;
    bool noCapture                  = false;
    bool discardable                = false;

    [[nodiscard]] constexpr bool hasResidualFacts() const noexcept {
        return returnConsumed != HirConsumedState::Unknown || nonNull || noAlias || readOnly ||
               noCapture || discardable;
    }
};

/// Optional proof tied to one safety-sensitive expression and its operands.
struct HirSafetyProof {
    HirSafetyProofKind kind = HirSafetyProofKind::BoundsValid;
    HirExprId operand_a = kInvalidHirExpr;
    HirExprId operand_b = kInvalidHirExpr;
    HirExprId operand_c = kInvalidHirExpr;
    bool present = false;

    [[nodiscard]] constexpr bool hasResidualFacts() const noexcept {
        return present;
    }
};

/// Parallel residual-fact tables attached to the existing HIR nodes.
///
/// The tables are intentionally side tables: the HIR expression/Fn records stay
/// ownership-free, so code without ownership remains valid with empty tables.
/// `visitExpr`, `dump` and the current emitters ignore ownership attributes.
class HirAttrs {
    memory::DynArray<HirSlotAttrs> slots_;
    memory::DynArray<HirCallAttrs> calls_;
    memory::DynArray<HirFnAttrs> fns_;
    memory::DynArray<HirSafetyProof> safety_proofs_;
    memory::Arena *arena_ = nullptr;

public:
    explicit HirAttrs(memory::Arena &arena)
        : slots_(arena), calls_(arena), fns_(arena), safety_proofs_(arena), arena_(&arena) {}

    HirAttrs(const HirAttrs &)                = delete;
    HirAttrs &operator=(const HirAttrs &)     = delete;
    HirAttrs(HirAttrs &&) noexcept            = default;
    HirAttrs &operator=(HirAttrs &&) noexcept = default;

    HirSlotAttrs &slot(HirSlotId id) {
        if (id >= slots_.size())
            slots_.resize(id + 1U);
        return slots_[id];
    }

    [[nodiscard]] const HirSlotAttrs *trySlot(HirSlotId id) const noexcept {
        return id < slots_.size() ? &slots_[id] : nullptr;
    }

    HirCallAttrs &call(HirExprId id) {
        if (id >= calls_.size())
            calls_.resizeEmplace(id + 1U, *arena_);
        return calls_[id];
    }

    [[nodiscard]] const HirCallAttrs *tryCall(HirExprId id) const noexcept {
        return id < calls_.size() ? &calls_[id] : nullptr;
    }

    HirFnAttrs &fn(size_t index) {
        if (index >= fns_.size())
            fns_.resize(index + 1U);
        return fns_[index];
    }

    [[nodiscard]] const HirFnAttrs *tryFn(size_t index) const noexcept {
        return index < fns_.size() ? &fns_[index] : nullptr;
    }

    HirSafetyProof &safetyProof(HirExprId id) {
        if (id >= safety_proofs_.size())
            safety_proofs_.resize(id + 1U);
        return safety_proofs_[id];
    }

    [[nodiscard]] const HirSafetyProof *trySafetyProof(HirExprId id) const noexcept {
        return id < safety_proofs_.size() ? &safety_proofs_[id] : nullptr;
    }

    [[nodiscard]] size_t slotCount() const noexcept {
        return slots_.size();
    }
    [[nodiscard]] size_t callCount() const noexcept {
        return calls_.size();
    }
    [[nodiscard]] size_t fnCount() const noexcept {
        return fns_.size();
    }
    [[nodiscard]] size_t safetyProofCount() const noexcept {
        return safety_proofs_.size();
    }
};

} // namespace zith::hir
