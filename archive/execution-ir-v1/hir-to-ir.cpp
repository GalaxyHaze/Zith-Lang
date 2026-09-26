#include "hir-to-ir.hpp"

#include "hir/hir-expr.hpp"
#include "symbols/symbol-id.hpp"
#include "types/type-kind.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace zith::ir {
namespace {

constexpr uint16_t kUnassignedReg = 0xFFFF;

std::string_view sourceName(std::string_view linkage) {
    auto paren = linkage.find('(');
    if (paren != std::string_view::npos)
        linkage = linkage.substr(0, paren);
    auto dot = linkage.rfind('.');
    if (dot != std::string_view::npos)
        linkage = linkage.substr(dot + 1);
    return linkage;
}

uint16_t u16(size_t value, std::string_view message, LowerResult *result) {
    if (value > 0xFFFFU) {
        result->ok      = false;
        result->message = std::string(message);
        return 0;
    }
    return static_cast<uint16_t>(value);
}

struct LowerState {
    memory::Arena &arena;
    const hir::HirModule &hir;
    const memory::StringInterner &interner;
    const types::TypeIntern &types;
    Module &out;

    const hir::HirFunction *hirFn = nullptr;
    Function *fn                  = nullptr;
    memory::DynArray<uint16_t> regs;
    LowerResult *result = nullptr;
};

void emit(LowerState &state, Instr instr) {
    state.fn->body.push(instr);
}

uint16_t lowerOperand(LowerState &state, hir::HirExprId id) {
    if (id >= state.regs.size())
        return 0;
    if (state.regs[id] != kUnassignedReg)
        return state.regs[id];

    const auto &expr = state.hir.getExpr(id);
    if (std::holds_alternative<hir::HirLiteral>(expr)) {
        const auto &lit = std::get<hir::HirLiteral>(expr);
        const auto dest = state.fn->registerCount++;
        if (state.types.kindOf(lit.type) == types::TypeKind::Ptr) {
            const auto text = state.interner.lookup(lit.str_val);
            size_t index    = 0;
            for (; index < state.out.strings.size(); ++index)
                if (state.out.strings[index] == text)
                    break;
            if (index == state.out.strings.size())
                state.out.strings.push(text);
            emit(state,
                 Instr{Op::LoadString, dest, 0, 0, u16(index, "too many strings", state.result)});
        } else {
            const auto index = state.out.constants.size();
            if (!u16(index, "too many constants", state.result)) {
            }
            state.out.constants.push(lit.i);
            emit(state, Instr{Op::LoadConst, dest, 0, 0, static_cast<uint16_t>(index)});
        }
        state.regs[id] = dest;
        return dest;
    }
    if (std::holds_alternative<hir::HirVar>(expr)) {
        const auto &var = std::get<hir::HirVar>(expr);
        for (size_t p = 0; p < state.fn->paramCount; ++p) {
            if (p < state.hirFn->param_names.size() && state.hirFn->param_names[p] == var.name) {
                state.regs[id] = static_cast<uint16_t>(p);
                return static_cast<uint16_t>(p);
            }
        }
        state.result->ok      = false;
        state.result->message = "unsupported variable in execution IR lowering";
        return 0;
    }
    if (std::holds_alternative<hir::HirBinary>(expr)) {
        const auto &bin = std::get<hir::HirBinary>(expr);
        const auto lhs  = lowerOperand(state, bin.lhs);
        const auto rhs  = lowerOperand(state, bin.rhs);
        const auto dest = state.fn->registerCount++;
        Op op           = Op::Trap;
        switch (bin.op) {
        case hir::HirBinaryOp::Add:
            op = Op::Add;
            break;
        case hir::HirBinaryOp::Sub:
            op = Op::Sub;
            break;
        case hir::HirBinaryOp::Mul:
            op = Op::Mul;
            break;
        case hir::HirBinaryOp::Div:
            op = Op::Div;
            break;
        case hir::HirBinaryOp::Rem:
            op = Op::Rem;
            break;
        case hir::HirBinaryOp::Eq:
            op = Op::Eq;
            break;
        case hir::HirBinaryOp::Ne:
            op = Op::Ne;
            break;
        case hir::HirBinaryOp::Lt:
            op = Op::Lt;
            break;
        case hir::HirBinaryOp::Le:
            op = Op::Le;
            break;
        case hir::HirBinaryOp::Gt:
            op = Op::Gt;
            break;
        case hir::HirBinaryOp::Ge:
            op = Op::Ge;
            break;
        case hir::HirBinaryOp::And:
            op = Op::And;
            break;
        case hir::HirBinaryOp::Or:
            op = Op::Or;
            break;
        case hir::HirBinaryOp::Xor:
            op = Op::Xor;
            break;
        case hir::HirBinaryOp::Shl:
            op = Op::Shl;
            break;
        case hir::HirBinaryOp::Shr:
            op = Op::Shr;
            break;
        case hir::HirBinaryOp::Invalid:
            break;
        }
        emit(state, Instr{op, dest, lhs, rhs, 0});
        state.regs[id] = dest;
        return dest;
    }
    if (std::holds_alternative<hir::HirCall>(expr)) {
        const auto &call     = std::get<hir::HirCall>(expr);
        uint16_t calleeIndex = 0;
        bool isExtern        = false;
        bool found           = false;
        if (call.resolved_fn != symbols::kInvalidSym) {
            for (size_t f = 0; f < state.hir.getFnCount(); ++f) {
                if (state.hir.getFn(f).sym_id != call.resolved_fn)
                    continue;
                for (size_t candidate = 0; candidate < state.out.functions.size(); ++candidate) {
                    if (state.out.functions[candidate].name ==
                        sourceName(state.interner.lookup(state.hir.getFn(f).name))) {
                        calleeIndex = static_cast<uint16_t>(candidate);
                        found       = true;
                        break;
                    }
                }
                if (found)
                    break;
            }
        }
        if (!found) {
            if (const auto *var = std::get_if<hir::HirVar>(&state.hir.getExpr(call.callee))) {
                const auto name = state.interner.lookup(var->name);
                for (size_t e = 0; e < state.out.externs.size(); ++e)
                    if (state.out.externs[e] == name) {
                        calleeIndex = static_cast<uint16_t>(e);
                        isExtern    = true;
                        found       = true;
                        break;
                    }
            }
        }
        if (!found) {
            state.result->ok      = false;
            state.result->message = "unsupported call target in execution IR lowering";
            return 0;
        }
        const uint16_t arg0 = call.args.size() > 0 ? lowerOperand(state, call.args[0]) : 0;
        const uint16_t arg1 = call.args.size() > 1 ? lowerOperand(state, call.args[1]) : 0;
        const auto dest     = state.fn->registerCount++;
        emit(state, Instr{isExtern ? Op::CallExtern : Op::CallFn, dest, arg0, arg1, calleeIndex});
        state.regs[id] = dest;
        return dest;
    }
    if (std::holds_alternative<hir::HirSlotLoad>(expr)) {
        const auto &load = std::get<hir::HirSlotLoad>(expr);
        const auto dest  = state.fn->registerCount++;
        emit(state, Instr{Op::SlotLoad, dest, 0, 0, static_cast<uint16_t>(load.slot)});
        state.regs[id] = dest;
        return dest;
    }
    if (std::holds_alternative<hir::HirUnary>(expr)) {
        const auto &un     = std::get<hir::HirUnary>(expr);
        const auto operand = lowerOperand(state, un.operand);
        const auto dest    = state.fn->registerCount++;
        Op op              = Op::Trap;
        if (un.op == hir::HirUnaryOp::Neg)
            op = Op::Neg;
        else if (un.op == hir::HirUnaryOp::Not)
            op = Op::Not;
        else if (un.op == hir::HirUnaryOp::BitNot)
            op = Op::BitNot;
        emit(state, Instr{op, dest, operand, 0, 0});
        state.regs[id] = dest;
        return dest;
    }
    state.result->ok      = false;
    state.result->message = "unsupported HIR operand in execution IR lowering";
    return 0;
}

} // namespace

LowerResult lowerModule(const hir::HirModule &hir, const memory::StringInterner &interner,
                        const types::TypeIntern &types, memory::Arena &arena, Module &out) {
    LowerResult result{true, {}};

    for (size_t i = 0; i < hir.getFnCount(); ++i) {
        const auto &hirFn  = hir.getFn(i);
        const auto linkage = interner.lookup(hirFn.name);
        if (hirFn.blocks.empty()) {
            const auto name = sourceName(linkage);
            bool found      = false;
            for (size_t e = 0; e < out.externs.size(); ++e)
                if (out.externs[e] == name)
                    found = true;
            if (!found)
                out.externs.push(name);
            continue;
        }
        if (hirFn.decl_id == ast::kInvalidDecl)
            continue;

        auto &fn      = out.functions.emplace(arena);
        fn.name       = sourceName(linkage);
        fn.paramCount = static_cast<uint16_t>(hirFn.param_names.size());
        fn.registerCount =
            static_cast<uint16_t>(hirFn.param_names.size() > 0 ? hirFn.param_names.size() : 1);

        LowerState state{arena,  hir,    interner, types,
                         out,    &hirFn, &fn,      memory::DynArray<uint16_t>(arena),
                         &result};
        state.regs.resize(hir.exprCount(), kUnassignedReg);

        for (const auto &hirBlock : hirFn.blocks) {
            for (auto inst : hirBlock.insts) {
                const auto &expr = hir.getExpr(inst);
                if (std::holds_alternative<hir::HirSlotAlloca>(expr)) {
                    const auto &alloca = std::get<hir::HirSlotAlloca>(expr);
                    fn.slotCount =
                        std::max<uint16_t>(fn.slotCount, static_cast<uint16_t>(alloca.slot) + 1);
                } else if (std::holds_alternative<hir::HirSlotStore>(expr)) {
                    const auto &store = std::get<hir::HirSlotStore>(expr);
                    const auto value  = lowerOperand(state, store.value);
                    fn.slotCount =
                        std::max<uint16_t>(fn.slotCount, static_cast<uint16_t>(store.slot) + 1);
                    emit(state,
                         Instr{Op::SlotStore, 0, value, 0, static_cast<uint16_t>(store.slot)});
                } else if (std::holds_alternative<hir::HirRet>(expr)) {
                    // Terminator ret is emitted separately below.
                } else {
                    (void)lowerOperand(state, inst);
                }
            }

            if (hirBlock.terminator != hir::kInvalidHirExpr) {
                const auto &terminator = hir.getExpr(hirBlock.terminator);
                if (std::holds_alternative<hir::HirJump>(terminator)) {
                    const auto &jump = std::get<hir::HirJump>(terminator);
                    emit(state, Instr{Op::Jump, 0, 0, 0, static_cast<uint16_t>(jump.target)});
                } else if (std::holds_alternative<hir::HirBranch>(terminator)) {
                    const auto &branch = std::get<hir::HirBranch>(terminator);
                    const auto cond    = lowerOperand(state, branch.cond);
                    emit(state,
                         Instr{Op::Branch, 0, cond, 0, static_cast<uint16_t>(branch.then_block)});
                    emit(state, Instr{Op::Jump, 0, 0, 0, static_cast<uint16_t>(branch.else_block)});
                } else if (std::holds_alternative<hir::HirRet>(terminator)) {
                    const auto &ret = std::get<hir::HirRet>(terminator);
                    if (ret.value != hir::kInvalidHirExpr) {
                        fn.returnRegister   = lowerOperand(state, ret.value);
                        fn.returnIsExitCode = true;
                    } else {
                        fn.returnIsExitCode = false;
                    }
                    emit(state, Instr{Op::Ret, 0, 0, 0, 0});
                }
            }
        }
    }

    return result;
}

} // namespace zith::ir
