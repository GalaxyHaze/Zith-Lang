#include "vm/hir-to-vm.hpp"

#include "common/ast-ids.hpp"
#include "common/overloaded.hpp"
#include "types/type-kind.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace zith::vm {
namespace {

constexpr std::uint16_t kUnassignedReg = 0xFFFFU;

std::string_view sourceName(std::string_view linkage) {
    auto paren = linkage.find('(');
    if (paren != std::string_view::npos)
        linkage = linkage.substr(0, paren);
    auto dot = linkage.rfind('.');
    if (dot != std::string_view::npos)
        linkage = linkage.substr(dot + 1);
    return linkage;
}

bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

auto isUserFunction(std::string_view linkage, const hir::HirFunction &hirFn) -> bool {
    if (hirFn.blocks.empty() || hirFn.decl_id == ast::kInvalidDecl)
        return false;
    const std::string_view name = sourceName(linkage);
    if (name == "main")
        return true;
    return !startsWith(linkage, "std.") && !startsWith(linkage, "zith.");
}

auto u16(std::size_t value, LowerResult *result) -> std::uint16_t {
    if (value > 0xFFFFU) {
        result->ok      = false;
        result->message = "v2 module table exceeds 65535 entries";
        return 0;
    }
    return static_cast<std::uint16_t>(value);
}

auto findOrAddString(Module &out, std::string_view text, LowerResult *result) -> std::uint16_t {
    for (std::size_t i = 0; i < out.strings.size(); ++i)
        if (out.strings[i] == text)
            return u16(i, result);
    out.strings.push(text);
    return u16(out.strings.size() - 1, result);
}

auto findOrAddExtern(Module &out, std::string_view name, LowerResult *result) -> std::uint16_t {
    for (std::size_t i = 0; i < out.externs.size(); ++i)
        if (out.externs[i] == name)
            return u16(i, result);
    out.externs.push(name);
    return u16(out.externs.size() - 1, result);
}

struct LowerState {
    memory::Arena &arena;
    const hir::HirModule &hir;
    const memory::StringInterner &interner;
    const types::TypeIntern &types;
    Module &out;

    const hir::HirFunction *hirFn = nullptr;
    Function *fn                  = nullptr;
    memory::DynArray<std::uint16_t> regs;
    memory::DynArray<std::uint16_t> slotRegs;
    LowerResult *result = nullptr;
};

void emit(LowerState &state, Instr instr) {
    state.fn->body.push(instr);
}

auto valueTypeOf(const types::TypeIntern &types, types::TypeId type) -> ValueType {
    switch (types.kindOf(type)) {
    case types::TypeKind::Int:
    case types::TypeKind::Bool:
    case types::TypeKind::Char:
        return ValueType::I64;
    case types::TypeKind::Float:
        return ValueType::F64;
    case types::TypeKind::Ptr:
    case types::TypeKind::String:
        return ValueType::Ptr;
    case types::TypeKind::Slice:
        return ValueType::Slice;
    case types::TypeKind::Void:
        return ValueType::Void;
    default:
        return ValueType::Void;
    }
}

auto newReg(LowerState &state, types::TypeId type) -> std::uint16_t {
    const auto reg = static_cast<std::uint16_t>(state.fn->regCount++);
    state.fn->regTypes.push(valueTypeOf(state.types, type));
    return reg;
}

auto lowerOperand(LowerState &state, hir::HirExprId id) -> std::uint16_t;

auto lowerLiteral(LowerState &state, const hir::HirLiteral &lit) -> std::uint16_t {
    const auto reg = newReg(state, lit.type);
    switch (state.types.kindOf(lit.type)) {
    case types::TypeKind::Bool:
    case types::TypeKind::Char:
    case types::TypeKind::Int:
        emit(state, Instr::withImm(Op::LoadConstI32, reg, static_cast<std::uint16_t>(lit.i)));
        break;
    case types::TypeKind::Float: {
        const auto rawIndex = state.out.f64Constants.size();
        const auto index    = u16(rawIndex, state.result);
        if (index == 0 && rawIndex != 0)
            return reg;
        state.out.f64Constants.push(lit.f);
        emit(state, Instr::withImm(Op::LoadConstF64, reg, index));
        break;
    }
    case types::TypeKind::Ptr:
    case types::TypeKind::String: {
        const auto index = findOrAddString(state.out, state.interner.lookup(lit.str_val),
                                           state.result);
        emit(state, Instr::withImm(Op::LoadString, reg, index));
        break;
    }
    default:
        state.result->ok      = false;
        state.result->message = "unsupported literal type in v2 lowering";
        break;
    }
    return reg;
}

auto lowerStringSlicePointer(LowerState &state, const hir::HirMakeSlice &slice)
    -> std::uint16_t {
    if (slice.is_pointer) {
        const auto obj = lowerOperand(state, slice.object);
        return obj;
    }

    const auto &objExpr = state.hir.getExpr(slice.object);
    const auto *lit     = std::get_if<hir::HirLiteral>(&objExpr);
    if (lit == nullptr || state.types.kindOf(lit->type) != types::TypeKind::Ptr)
        return kUnassignedReg;

    const auto text = state.interner.lookup(lit->str_val);
    if (text.empty())
        return kUnassignedReg;
    const auto index = findOrAddString(state.out, text, state.result);
    const auto reg   = newReg(state, lit->type);
    emit(state, Instr::withImm(Op::LoadString, reg, index));
    return reg;
}

auto lowerCall(LowerState &state, const hir::HirCall &call) -> std::uint16_t {
    if (call.resolved_fn == symbols::kInvalidSym) {
        state.result->ok      = false;
        state.result->message = "unsupported unresolved call target in v2 lowering";
        return kUnassignedReg;
    }

    for (std::size_t f = 0; f < state.hir.getFnCount(); ++f) {
        const auto &hirFn = state.hir.getFn(f);
        if (hirFn.sym_id != call.resolved_fn)
            continue;
        const auto linkage = state.interner.lookup(hirFn.name);
        const auto name    = sourceName(linkage);
        if (startsWith(linkage, "std.io.console.println")) {
            if (call.args.empty())
                return newReg(state, types::kVoidType);
            const auto &argExpr = state.hir.getExpr(call.args[0]);
            const auto *slice   = std::get_if<hir::HirMakeSlice>(&argExpr);
            if (slice != nullptr) {
                const auto ptr = lowerStringSlicePointer(state, *slice);
                if (ptr != kUnassignedReg) {
                    while (state.fn->regTypes.size() < 5)
                        state.fn->regTypes.push(ValueType::I64);
                    const auto externIndex = findOrAddExtern(state.out, "puts", state.result);
                    const auto scratch     = newReg(state, types::kVoidType);
                    emit(state, Instr::callExtern(Op::CallExtern, scratch, ptr, 0, externIndex));
                    return scratch;
                }
            }
            continue;
        }
        if (name == "main" || hirFn.blocks.empty() || hirFn.decl_id == ast::kInvalidDecl) {
            if (name == "puts" || name == "putchar" || name == "malloc" || name == "free" ||
                name == "snprintf" || name == "strlen" || name == "memcpy") {
                if (call.args.size() > 4)
                    return kUnassignedReg;
                const std::uint16_t arg0 =
                    call.args.size() > 0 ? lowerOperand(state, call.args[0]) : 0;
                const std::uint16_t arg1 =
                    call.args.size() > 1 ? lowerOperand(state, call.args[1]) : 0;
                const std::uint16_t arg2 =
                    call.args.size() > 2 ? lowerOperand(state, call.args[2]) : 0;
                const std::uint16_t arg3 =
                    call.args.size() > 3 ? lowerOperand(state, call.args[3]) : 0;
                while (state.fn->regTypes.size() < 5)
                    state.fn->regTypes.push(ValueType::I64);
                const auto externIndex = findOrAddExtern(state.out, name, state.result);
                const auto reg         = newReg(state, call.fn_type);
                emit(state, Instr::callExtern(Op::CallExtern, reg, arg0, arg1, externIndex, arg2,
                                              arg3));
                return reg;
            }
            continue;
        }
        if (startsWith(linkage, "std.") || startsWith(linkage, "zith."))
            continue;
        for (std::size_t candidate = 0; candidate < state.out.functions.size(); ++candidate) {
            if (state.out.functions[candidate].name == name) {
                if (call.args.size() > 2)
                    return kUnassignedReg;
                const std::uint16_t arg0 =
                    call.args.size() > 0 ? lowerOperand(state, call.args[0]) : 0;
                const std::uint16_t arg1 =
                    call.args.size() > 1 ? lowerOperand(state, call.args[1]) : 0;
                const auto fnIndex = u16(candidate, state.result);
                while (state.fn->regTypes.size() < 4) {
                    state.fn->regTypes.push(ValueType::I64);
                    state.fn->regCount = static_cast<std::uint16_t>(state.fn->regTypes.size());
                }
                const auto reg = newReg(state, call.fn_type);
                emit(state, Instr{Op::CallFn, reg, arg0, arg1, fnIndex});
                return reg;
            }
        }
    }

    state.result->ok      = false;
    state.result->message = "unsupported resolved call target in v2 lowering";
    return 0;
}

auto lowerOperand(LowerState &state, hir::HirExprId id) -> std::uint16_t {
    if (id >= state.regs.size())
        return kUnassignedReg;
    if (state.regs[id] != kUnassignedReg)
        return state.regs[id];

    const auto &expr = state.hir.getExpr(id);
    const auto reg = hir::visitExpr(
        expr,
        common::overloaded{
            [&](const hir::HirLiteral &lit) { return lowerLiteral(state, lit); },
            [&](const hir::HirVar &var) -> std::uint16_t {
                for (std::size_t p = 0; p < state.hirFn->param_names.size(); ++p)
                    if (state.hirFn->param_names[p] == var.name) {
                        state.regs[id] = static_cast<std::uint16_t>(p);
                        return static_cast<std::uint16_t>(p);
                    }
                state.result->ok      = false;
                state.result->message = "unsupported HIR variable in v2 lowering";
                return kUnassignedReg;
            },
            [&](const hir::HirBinary &bin) -> std::uint16_t {
                const auto lhs = lowerOperand(state, bin.lhs);
                const auto rhs = lowerOperand(state, bin.rhs);
                if (lhs == kUnassignedReg || rhs == kUnassignedReg)
                    return kUnassignedReg;
                Op op = Op::Trap;
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
                case hir::HirBinaryOp::Or:
                case hir::HirBinaryOp::Xor:
                case hir::HirBinaryOp::Shl:
                case hir::HirBinaryOp::Shr:
                case hir::HirBinaryOp::Invalid:
                    op = Op::Trap;
                    break;
                }
                if (op == Op::Trap) {
                    state.result->ok      = false;
                    state.result->message = "unsupported HIR binary operator in v2 lowering";
                    return kUnassignedReg;
                }
                const auto dst = newReg(state, bin.type);
                emit(state, Instr::simple(op, dst, lhs, rhs));
                return dst;
            },
            [&](const hir::HirUnary &un) -> std::uint16_t {
                const auto operand = lowerOperand(state, un.operand);
                if (operand == kUnassignedReg)
                    return kUnassignedReg;
                Op op = Op::Trap;
                if (un.op == hir::HirUnaryOp::Neg)
                    op = Op::Neg;
                else if (un.op == hir::HirUnaryOp::Not)
                    op = Op::Not;
                if (op == Op::Trap) {
                    state.result->ok      = false;
                    state.result->message = "unsupported HIR unary operator in v2 lowering";
                    return kUnassignedReg;
                }
                const auto dst = newReg(state, un.type);
                emit(state, Instr::simple(op, dst, operand, 0));
                return dst;
            },
            [&](const hir::HirSlotLoad &load) -> std::uint16_t {
                if (load.slot >= state.fn->regCount)
                    state.fn->regCount = static_cast<std::uint16_t>(load.slot + 1);
                while (state.fn->regTypes.size() <= load.slot) {
                    state.fn->regTypes.push(ValueType::I64);
                    state.fn->regCount = static_cast<std::uint16_t>(state.fn->regTypes.size());
                }
                return state.slotRegs[load.slot] != kUnassignedReg
                           ? state.slotRegs[load.slot]
                           : static_cast<std::uint16_t>(load.slot);
            },
            [&](const hir::HirSlotAddr &addr) -> std::uint16_t {
                while (state.fn->regTypes.size() <= addr.slot) {
                    state.fn->regTypes.push(ValueType::I64);
                    state.fn->regCount = static_cast<std::uint16_t>(state.fn->regTypes.size());
                }
                return static_cast<std::uint16_t>(addr.slot);
            },
            [&](const hir::HirCast &cast) -> std::uint16_t {
                const auto value = lowerOperand(state, cast.value);
                if (value == kUnassignedReg)
                    return kUnassignedReg;
                return value;
            },
            [&](const hir::HirIndex &index) -> std::uint16_t {
                const auto object = lowerOperand(state, index.object);
                if (object == kUnassignedReg)
                    return kUnassignedReg;
                const auto &indexExpr = state.hir.getExpr(index.index);
                const auto *indexLit  = std::get_if<hir::HirLiteral>(&indexExpr);
                if (indexLit == nullptr) {
                    state.result->ok      = false;
                    state.result->message = "unsupported dynamic index in v2 lowering";
                    return kUnassignedReg;
                }
                const auto ptr = newReg(state, index.obj_type);
                emit(state, Instr{Op::FieldPtr, ptr, object, 0,
                                  static_cast<std::uint16_t>(indexLit->i)});
                const auto count = newReg(state, types::kCharType);
                emit(state, Instr::withImm(Op::LoadConstI32, count, 1));
                const auto dst = newReg(state, index.type);
                emit(state, Instr::simple(Op::LoadBytes, dst, ptr, count));
                return dst;
            },
            [&](const hir::HirLayoutIntrinsic &intrinsic) -> std::uint16_t {
                if (intrinsic.which != hir::HirLayoutIntrinsic::Which::PtrOf ||
                    intrinsic.operand == hir::kInvalidHirExpr)
                    return kUnassignedReg;
                return lowerOperand(state, intrinsic.operand);
            },
            [&](const hir::HirMakeSlice &slice) -> std::uint16_t {
                const auto ptr = lowerStringSlicePointer(state, slice);
                if (ptr != kUnassignedReg)
                    return ptr;
                const auto lo = lowerOperand(state, slice.lo);
                const auto hi = lowerOperand(state, slice.hi);
                (void)lo;
                (void)hi;
                return kUnassignedReg;
            },
            [&](const hir::HirCall &call) { return lowerCall(state, call); },
            [](const auto &) -> std::uint16_t { return kUnassignedReg; },
        });
    if (reg != kUnassignedReg)
        state.regs[id] = reg;
    return reg;
}

} // namespace

auto lowerModule(const hir::HirModule &hir, const memory::StringInterner &interner,
                 const types::TypeIntern &types, memory::Arena &arena, Module &out)
    -> LowerResult {
    LowerResult result{true, {}};

    for (std::size_t i = 0; i < hir.getFnCount(); ++i) {
        const auto &hirFn  = hir.getFn(i);
        const auto linkage = interner.lookup(hirFn.name);
        if (!isUserFunction(linkage, hirFn)) {
            if (hirFn.blocks.empty() && hirFn.decl_id == ast::kInvalidDecl)
                (void)findOrAddExtern(out, sourceName(linkage), &result);
            continue;
        }

        auto &fn      = out.functions.emplace(arena);
        fn.name       = sourceName(linkage);
        fn.paramCount = static_cast<std::uint16_t>(hirFn.param_names.size());
        fn.regCount   = static_cast<std::uint16_t>(
            std::max<std::size_t>(hirFn.param_names.size(), 1U));
        fn.returnType = valueTypeOf(types, hirFn.return_type);

        LowerState state{arena, hir,  interner, types, out, &hirFn, &fn,
                         memory::DynArray<std::uint16_t>(arena),
                         memory::DynArray<std::uint16_t>(arena), &result};
        state.regs.resize(hir.exprCount(), kUnassignedReg);
        state.slotRegs.resize(hir.exprCount(), kUnassignedReg);
        while (fn.regTypes.size() < fn.regCount)
            fn.regTypes.push(ValueType::I64);

        for (const auto &block : hirFn.blocks) {
            for (const auto instId : block.insts) {
                const auto &expr = hir.getExpr(instId);
                if (std::holds_alternative<hir::HirSlotAlloca>(expr)) {
                    const auto &alloca = std::get<hir::HirSlotAlloca>(expr);
                    while (fn.regTypes.size() <= alloca.slot) {
                        fn.regTypes.push(ValueType::Ptr);
                        fn.regCount = static_cast<std::uint16_t>(fn.regTypes.size());
                    }
                } else if (std::holds_alternative<hir::HirSlotStore>(expr)) {
                    const auto &store = std::get<hir::HirSlotStore>(expr);
                    while (fn.regTypes.size() <= store.slot) {
                        fn.regTypes.push(ValueType::Ptr);
                        fn.regCount = static_cast<std::uint16_t>(fn.regTypes.size());
                    }
                    const auto &valueExpr = hir.getExpr(store.value);
                    const bool aggregate =
                        std::holds_alternative<hir::HirArrayLiteral>(valueExpr) ||
                        std::holds_alternative<hir::HirStructLiteral>(valueExpr) ||
                        std::holds_alternative<hir::HirMakeSlice>(valueExpr) ||
                        std::holds_alternative<hir::HirSlotAddr>(valueExpr);
                    if (!aggregate) {
                        const auto value = lowerOperand(state, store.value);
                        if (value != kUnassignedReg)
                            state.slotRegs[store.slot] = value;
                    }
                } else {
                    (void)lowerOperand(state, instId);
                }
            }
            if (block.terminator == hir::kInvalidHirExpr)
                continue;
            const auto &terminator = hir.getExpr(block.terminator);
            if (const auto *ret = std::get_if<hir::HirRet>(&terminator)) {
                const auto value = ret->value == hir::kInvalidHirExpr
                                       ? 0
                                       : lowerOperand(state, ret->value);
                if (value == kUnassignedReg) {
                    result.ok      = false;
                    result.message = "unsupported return value in v2 lowering";
                    return result;
                }
                emit(state, Instr{Op::Ret, static_cast<std::uint16_t>(value), 0, 0, 0});
            }
        }
    }

    return result;
}

} // namespace zith::vm
