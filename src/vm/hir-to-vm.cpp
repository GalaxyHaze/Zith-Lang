#include "vm/hir-to-vm.hpp"

#include "common/ast-ids.hpp"
#include "common/overloaded.hpp"
#include "types/type-kind.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

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
    memory::DynArray<std::uint16_t> paramRegs;
    memory::DynArray<std::uint16_t> slotRegs;
    memory::DynArray<std::uint8_t> slotWidths;
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

auto newTypedReg(LowerState &state, ValueType vt) -> std::uint16_t {
    const auto reg = static_cast<std::uint16_t>(state.fn->regCount++);
    state.fn->regTypes.push(vt);
    return reg;
}

auto newReg(LowerState &state, types::TypeId type) -> std::uint16_t {
    return newTypedReg(state, valueTypeOf(state.types, type));
}

auto lowerOperand(LowerState &state, hir::HirExprId id) -> std::uint16_t;

auto valueRegisterWidth(const types::TypeIntern &types, types::TypeId type) -> std::size_t {
    return types.kindOf(type) == types::TypeKind::Slice ? 2U : 1U;
}

auto slotRegister(const LowerState &state, hir::HirSlotId slot) -> std::uint16_t {
    if (slot >= state.slotRegs.size())
        return kUnassignedReg;
    return state.slotRegs[slot];
}

auto scalarElementSize(const types::TypeIntern &types, types::TypeId type) -> std::size_t {
    switch (types.kindOf(type)) {
    case types::TypeKind::Int:
    case types::TypeKind::Bool:
    case types::TypeKind::Char:
    case types::TypeKind::Ptr:
    case types::TypeKind::String:
        return sizeof(std::int64_t);
    default:
        return 0;
    }
}

auto arrayElementSize(const types::TypeIntern &types, types::TypeId type) -> std::size_t {
    const auto *array = std::get_if<types::TypeArray>(&types.lookup(type));
    return array != nullptr ? scalarElementSize(types, array->elem) : 0;
}

auto lowerArrayLiteral(LowerState &state, const hir::HirArrayLiteral &literal)
    -> std::uint16_t {
    const auto *array      = std::get_if<types::TypeArray>(&state.types.lookup(literal.type));
    const auto elementSize = arrayElementSize(state.types, literal.type);
    if (array == nullptr || elementSize == 0 || literal.elements.size() != array->count ||
        array->count > std::numeric_limits<std::size_t>::max() / elementSize ||
        array->count * elementSize > 0xFFFFU) {
        return kUnassignedReg;
    }

    const auto count = newTypedReg(state, ValueType::I64);
    const auto align = newTypedReg(state, ValueType::I64);
    emit(state, Instr::withImm(Op::LoadConstI64, count,
                               static_cast<std::uint16_t>(array->count * elementSize)));
    emit(state, Instr::withImm(Op::LoadConstI64, align, 8));
    const auto base = newTypedReg(state, ValueType::Ptr);
    emit(state, Instr::simple(Op::AllocBytes, base, count, align));

    for (std::size_t i = 0; i < literal.elements.size(); ++i) {
        const auto value = lowerOperand(state, literal.elements[i]);
        if (value == kUnassignedReg)
            return kUnassignedReg;
        const auto address = newTypedReg(state, ValueType::Ptr);
        emit(state,
             Instr{Op::FieldPtr, address, base, 0, static_cast<std::uint16_t>(i * elementSize)});
        emit(state, Instr::simple(Op::StoreI64, address, value));
    }
    return base;
}

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
    const auto &objExpr = state.hir.getExpr(slice.object);
    const auto *lit     = std::get_if<hir::HirLiteral>(&objExpr);
    if (lit != nullptr) {
        const auto text = state.interner.lookup(lit->str_val);
        if (!text.empty()) {
            const auto kind = state.types.kindOf(lit->type);
            if (slice.is_pointer || kind == types::TypeKind::Ptr ||
                kind == types::TypeKind::String) {
                const auto index = findOrAddString(state.out, text, state.result);
                const auto reg   = newReg(state, lit->type);
                emit(state, Instr::withImm(Op::LoadString, reg, index));
                return reg;
            }
        }
    }

    if (slice.is_pointer)
        return lowerOperand(state, slice.object);

    return kUnassignedReg;
}

auto lowerStringSlicePointerAndLength(LowerState &state, const hir::HirMakeSlice &slice,
                                      std::uint16_t &outLength) -> std::uint16_t {
    const auto &objExpr = state.hir.getExpr(slice.object);
    const auto *lit     = std::get_if<hir::HirLiteral>(&objExpr);
    if (lit != nullptr) {
        const auto text = state.interner.lookup(lit->str_val);
        if (!text.empty()) {
            const auto kind = state.types.kindOf(lit->type);
            if (slice.is_pointer || kind == types::TypeKind::Ptr ||
                kind == types::TypeKind::String) {
                const auto index = findOrAddString(state.out, text, state.result);
                const auto reg   = newReg(state, lit->type);
                emit(state, Instr::withImm(Op::LoadString, reg, index));

                const auto lenReg = newTypedReg(state, ValueType::I64);
                emit(state, Instr::withImm(Op::LoadConstI64, lenReg,
                                           static_cast<std::uint16_t>(text.size())));
                outLength = lenReg;
                return reg;
            }
        }
    }
    outLength = kUnassignedReg;
    return kUnassignedReg;
}

auto makeSlicePair(LowerState &state, std::uint16_t pointer, std::uint16_t length)
    -> std::uint16_t {
    const auto base = newTypedReg(state, ValueType::Ptr);
    (void)newTypedReg(state, ValueType::I64);
    emit(state, Instr::simple(Op::MakeSlice, base, pointer, length));
    return base;
}

auto lowerMakeSlice(LowerState &state, const hir::HirMakeSlice &slice) -> std::uint16_t {
    std::uint16_t literalLength = kUnassignedReg;
    const auto literalPointer = lowerStringSlicePointerAndLength(state, slice, literalLength);
    if (literalPointer != kUnassignedReg && literalLength != kUnassignedReg)
        return makeSlicePair(state, literalPointer, literalLength);

    const auto pointer = lowerOperand(state, slice.object);
    const auto lo      = lowerOperand(state, slice.lo);
    const auto hi      = lowerOperand(state, slice.hi);
    if (pointer == kUnassignedReg || lo == kUnassignedReg || hi == kUnassignedReg)
        return kUnassignedReg;

    const auto *sliceType = std::get_if<types::TypeSlice>(&state.types.lookup(slice.type));
    const auto elementSize =
        sliceType != nullptr ? scalarElementSize(state.types, sliceType->elem) : 0U;
    if (sliceType == nullptr || elementSize == 0U || elementSize > 0xFFFFU) {
        state.result->ok      = false;
        state.result->message = "unsupported slice element type in v2 lowering";
        return kUnassignedReg;
    }

    std::uint16_t dataPointer = pointer;
    const auto &loExpr        = state.hir.getExpr(slice.lo);
    const auto *loLiteral     = std::get_if<hir::HirLiteral>(&loExpr);
    if (loLiteral == nullptr || loLiteral->i != 0) {
        const auto scale = newTypedReg(state, ValueType::I64);
        emit(state, Instr::withImm(Op::LoadConstI64, scale,
                                   static_cast<std::uint16_t>(elementSize)));
        const auto byteOffset = newTypedReg(state, ValueType::I64);
        emit(state, Instr::simple(Op::Mul, byteOffset, lo, scale));
        dataPointer = newTypedReg(state, ValueType::Ptr);
        emit(state, Instr::simple(Op::Add, dataPointer, pointer, byteOffset));
    }

    const auto length = newTypedReg(state, ValueType::I64);
    emit(state, Instr::simple(Op::Sub, length, hi, lo));
    return makeSlicePair(state, dataPointer, length);
}

/// Counts the format placeholders in a literal message slice. A `#` followed
/// by the unit-separator byte is the escape for a literal `#`, so it does not
/// consume a value. This mirrors the escape handling in the stdlib
/// `std/io/console` `writeBuffer`.
auto slicePlaceholderCount(const LowerState &state, const hir::HirMakeSlice &slice)
    -> std::size_t {
    const auto &objExpr = state.hir.getExpr(slice.object);
    const auto *lit     = std::get_if<hir::HirLiteral>(&objExpr);
    if (lit == nullptr)
        return 0;
    const auto text = state.interner.lookup(lit->str_val);
    std::size_t count = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '#')
            continue;
        if (i + 1 < text.size() && text[i + 1] == '\x1f') {
            ++i;
            continue;
        }
        ++count;
    }
    return count;
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
        const bool is_println = startsWith(linkage, "std.io.console.println");
        const bool is_print   = startsWith(linkage, "std.io.console.print");
        if (is_println || is_print) {
            if (call.args.empty())
                return newReg(state, types::kVoidType);
            const auto &argExpr = state.hir.getExpr(call.args[0]);
            const auto *slice   = std::get_if<hir::HirMakeSlice>(&argExpr);
            // The fast path below forwards the literal message to `puts` or
            // `write_stdout`. It cannot render the variadic `Formatable`
            // values, so a call whose message has placeholders and carries
            // values must report unsupported instead of silently dropping the
            // formatted output. A message without placeholders ignores the
            // extra values, exactly like the native stdlib path.
            if (call.args.size() > 1 && slice != nullptr &&
                slicePlaceholderCount(state, *slice) > 0) {
                state.result->ok = false;
                state.result->message =
                    "variadic print formatting is not in the v2 lowering subset";
                return kUnassignedReg;
            }
            if (slice != nullptr) {
                const auto ptr = lowerStringSlicePointer(state, *slice);
                if (ptr != kUnassignedReg) {
                    while (state.fn->regTypes.size() < 5)
                        state.fn->regTypes.push(ValueType::I64);
                    if (is_println) {
                        const auto externIndex = findOrAddExtern(state.out, "puts", state.result);
                        const auto scratch     = newReg(state, types::kVoidType);
                        emit(state, Instr::callExtern(Op::CallExtern, scratch, ptr, 0, externIndex));
                        return scratch;
                    }
                    std::uint16_t len = kUnassignedReg;
                    (void)lowerStringSlicePointerAndLength(state, *slice, len);
                    if (len != kUnassignedReg) {
                        const auto externIndex =
                            findOrAddExtern(state.out, "write_stdout", state.result);
                        const auto scratch = newReg(state, types::kVoidType);
                        emit(state, Instr::callExtern(Op::CallExtern, scratch, ptr, len,
                                                      externIndex));
                        return scratch;
                    }
                }
            }
            continue;
        }
        if (name == "main" || hirFn.blocks.empty() || hirFn.decl_id == ast::kInvalidDecl) {
            if (name == "puts" || name == "putchar" || name == "malloc" || name == "free" ||
                name == "realloc" || name == "printf" || name == "snprintf" ||
                name == "strlen" || name == "memcpy" || name == "calloc" ||
                name == "strncmp" || name == "getchar") {
                if ((name == "printf" || name == "snprintf") && hirFn.isVariadic) {
                    const std::size_t fixedCount = name == "printf" ? 1U : 3U;
                    if (call.args.size() < fixedCount) {
                        state.result->ok      = false;
                        state.result->message = "variadic call has too few fixed arguments";
                        return kUnassignedReg;
                    }
                    const auto argBase = u16(state.fn->regTypes.size(), state.result);
                    const auto argCount = u16(call.args.size(), state.result);
                    if (!state.result->ok)
                        return kUnassignedReg;
                    for (std::size_t i = 0; i < call.args.size(); ++i)
                        state.fn->regTypes.push(ValueType::I64);
                    state.fn->regCount = static_cast<std::uint16_t>(state.fn->regTypes.size());
                    for (std::size_t i = 0; i < call.args.size(); ++i) {
                        const auto value = lowerOperand(state, call.args[i]);
                        if (value == kUnassignedReg)
                            return kUnassignedReg;
                        emit(state, Instr::simple(Op::Move,
                                                  static_cast<std::uint16_t>(argBase + i), value));
                    }
                    const auto externIndex = findOrAddExtern(state.out, name, state.result);
                    const auto resultReg   = newReg(state, call.fn_type);
                    emit(state, Instr::callExternRange(resultReg, argBase, argCount, externIndex));
                    return resultReg;
                }
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
                if (call.isVariadicSlice &&
                    (call.variadicSliceParam >= call.args.size() ||
                     call.variadicSliceParam >= call.argument_types.size() ||
                     state.types.kindOf(call.argument_types[call.variadicSliceParam]) !=
                         types::TypeKind::Slice)) {
                    state.result->ok      = false;
                    state.result->message = "invalid variadic slice plan in v2 lowering";
                    return kUnassignedReg;
                }

                std::vector<std::size_t> argumentWidths;
                argumentWidths.reserve(call.args.size());
                std::size_t argCount = 0;
                for (std::size_t i = 0; i < call.args.size(); ++i) {
                    const bool isPlannedSlice =
                        call.isVariadicSlice && i == call.variadicSliceParam;
                    const bool isSlice =
                        isPlannedSlice ||
                        (i < call.argument_types.size() &&
                         state.types.kindOf(call.argument_types[i]) == types::TypeKind::Slice);
                    const std::size_t width = isSlice ? 2U : 1U;
                    if (argCount + width > 0xFFFFU) {
                        state.result->ok      = false;
                        state.result->message = "v2 call argument range exceeds 65535 registers";
                        return kUnassignedReg;
                    }
                    argumentWidths.push_back(width);
                    argCount += width;
                }

                const auto fnIndex = u16(candidate, state.result);
                const std::uint16_t argBase =
                    argCount == 0 ? std::uint16_t{0} : u16(state.fn->regCount, state.result);
                if (!state.result->ok ||
                    static_cast<std::size_t>(state.fn->regCount) + argCount > 0xFFFFU) {
                    if (state.result->ok) {
                        state.result->ok      = false;
                        state.result->message =
                            "v2 caller registers exceed 65535 while preparing arguments";
                    }
                    return kUnassignedReg;
                }

                for (std::size_t i = 0; i < call.args.size(); ++i) {
                    const auto type = i < call.argument_types.size()
                                          ? call.argument_types[i]
                                          : types::kInvalidType;
                    state.fn->regTypes.push(argumentWidths[i] == 2U
                                               ? ValueType::Ptr
                                               : (type != types::kInvalidType
                                                      ? valueTypeOf(state.types, type)
                                                      : ValueType::I64));
                    if (argumentWidths[i] == 2U)
                        state.fn->regTypes.push(ValueType::I64);
                }
                state.fn->regCount = u16(state.fn->regTypes.size(), state.result);
                if (!state.result->ok)
                    return kUnassignedReg;

                if (argCount > 0) {
                    std::size_t argumentOffset = 0;
                    for (std::size_t i = 0; i < call.args.size(); ++i) {
                        const auto val = lowerOperand(state, call.args[i]);
                        if (val == kUnassignedReg)
                            return kUnassignedReg;
                        const auto destination =
                            u16(static_cast<std::size_t>(argBase) + argumentOffset, state.result);
                        emit(state, Instr::simple(Op::Move, destination, val));
                        if (argumentWidths[i] == 2U) {
                            if (static_cast<std::size_t>(val) + 1U >= state.fn->regCount) {
                                state.result->ok = false;
                                state.result->message =
                                    "slice argument pair exceeds v2 caller registers";
                                return kUnassignedReg;
                            }
                            emit(state, Instr::simple(
                                            Op::Move, static_cast<std::uint16_t>(destination + 1U),
                                            static_cast<std::uint16_t>(val + 1U)));
                        }
                        argumentOffset += argumentWidths[i];
                    }
                }
                const auto reg = newReg(state, call.fn_type);
                emit(state, Instr::callRange(reg, argBase, u16(argCount, state.result), fnIndex));
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
                        if (p >= state.paramRegs.size())
                            return kUnassignedReg;
                        state.regs[id] = state.paramRegs[p];
                        return state.paramRegs[p];
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
                    op = Op::BitAnd;
                    break;
                case hir::HirBinaryOp::Or:
                    op = Op::BitOr;
                    break;
                case hir::HirBinaryOp::Xor:
                    op = Op::BitXor;
                    break;
                case hir::HirBinaryOp::Shl:
                    op = Op::Shl;
                    break;
                case hir::HirBinaryOp::Shr:
                    op = Op::Shr;
                    break;
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
                else if (un.op == hir::HirUnaryOp::BitNot)
                    op = Op::BitNot;
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
                const auto base = slotRegister(state, load.slot);
                if (base == kUnassignedReg) {
                    state.result->ok      = false;
                    state.result->message = "unknown HIR slot in v2 lowering";
                }
                return base;
            },
            [&](const hir::HirSlotAddr &addr) -> std::uint16_t {
                const auto base = slotRegister(state, addr.slot);
                if (base == kUnassignedReg) {
                    state.result->ok      = false;
                    state.result->message = "unknown HIR slot address in v2 lowering";
                }
                return base;
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
                if (index.is_array) {
                    const auto *array =
                        std::get_if<types::TypeArray>(&state.types.lookup(index.obj_type));
                    const auto elementSize = arrayElementSize(state.types, index.obj_type);
                    if (array == nullptr || elementSize == 0 || array->count > 0xFFFFU ||
                        elementSize > 0xFFFFU) {
                        state.result->ok      = false;
                        state.result->message = "unsupported array index in v2 lowering";
                        return kUnassignedReg;
                    }
                    const auto indexReg = lowerOperand(state, index.index);
                    if (indexReg == kUnassignedReg)
                        return kUnassignedReg;
                    const auto dst = newReg(state, index.type);
                    emit(state,
                         Instr{Op::IndexLoad, dst, object, indexReg,
                               static_cast<std::uint16_t>(elementSize),
                               static_cast<std::uint16_t>(array->count)});
                    return dst;
                }

                const auto *slice =
                    std::get_if<types::TypeSlice>(&state.types.lookup(index.obj_type));
                if (slice != nullptr) {
                    const auto elementSize = scalarElementSize(state.types, slice->elem);
                    const auto indexReg    = lowerOperand(state, index.index);
                    if (elementSize == 0U || elementSize > 0xFFFFU ||
                        static_cast<std::size_t>(object) + 1U >= state.fn->regCount ||
                        indexReg == kUnassignedReg ||
                        indexReg >= state.fn->regCount) {
                        state.result->ok      = false;
                        state.result->message = "unsupported slice index in v2 lowering";
                        return kUnassignedReg;
                    }
                    const auto dst = newReg(state, index.type);
                    emit(state, Instr{Op::IndexLoad, dst, object, indexReg,
                                      static_cast<std::uint16_t>(elementSize),
                                      static_cast<std::uint16_t>(object + 1U), 1U});
                    return dst;
                }

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
                if (intrinsic.operand == hir::kInvalidHirExpr)
                    return kUnassignedReg;
                const auto operand = lowerOperand(state, intrinsic.operand);
                if (operand == kUnassignedReg)
                    return kUnassignedReg;
                if (intrinsic.which == hir::HirLayoutIntrinsic::Which::PtrOf)
                    return operand;
                if (intrinsic.which == hir::HirLayoutIntrinsic::Which::LengthOf) {
                    const auto kind = state.types.kindOf(intrinsic.operand_type);
                    if (kind == types::TypeKind::Slice) {
                        if (static_cast<std::size_t>(operand) + 1U >= state.fn->regCount)
                            return kUnassignedReg;
                        return static_cast<std::uint16_t>(operand + 1U);
                    }
                    if (const auto *array =
                            std::get_if<types::TypeArray>(&state.types.lookup(intrinsic.operand_type))) {
                        const auto length = newTypedReg(state, ValueType::I64);
                        emit(state, Instr::withImm(Op::LoadConstI64, length,
                                                   static_cast<std::uint16_t>(array->count)));
                        return length;
                    }
                }
                if (intrinsic.which == hir::HirLayoutIntrinsic::Which::LengthOf &&
                    intrinsic.string_length <= 0xFFFFU) {
                    const auto length = newTypedReg(state, ValueType::I64);
                    emit(state, Instr::withImm(
                                    Op::LoadConstI64, length,
                                    static_cast<std::uint16_t>(intrinsic.string_length)));
                    return length;
                }
                return kUnassignedReg;
            },
            [&](const hir::HirMakeSlice &slice) { return lowerMakeSlice(state, slice); },
            [&](const hir::HirArrayLiteral &literal) {
                return lowerArrayLiteral(state, literal);
            },
            [&](const hir::HirCall &call) { return lowerCall(state, call); },
            [](const auto &) -> std::uint16_t { return kUnassignedReg; },
        });
    if (reg != kUnassignedReg)
        state.regs[id] = reg;
    return reg;
}

auto prepareFunctionRegisters(LowerState &state) -> bool {
    std::size_t nextRegister = 0;
    if (state.hirFn->param_names.size() != state.hirFn->params.size()) {
        state.result->ok      = false;
        state.result->message = "HIR parameter names and types disagree in v2 lowering";
        return false;
    }

    state.paramRegs.resize(state.hirFn->params.size(), kUnassignedReg);
    for (std::size_t i = 0; i < state.hirFn->params.size(); ++i) {
        const auto type  = state.hirFn->params[i];
        const auto width = valueRegisterWidth(state.types, type);
        if (nextRegister + width > 0xFFFFU) {
            state.result->ok      = false;
            state.result->message = "v2 function parameter registers exceed 65535";
            return false;
        }
        state.paramRegs[i] = u16(nextRegister, state.result);
        state.fn->regTypes.push(valueTypeOf(state.types, type));
        if (width == 2U)
            state.fn->regTypes.push(ValueType::I64);
        nextRegister += width;
    }
    state.fn->paramCount = u16(nextRegister, state.result);
    if (!state.result->ok)
        return false;

    std::size_t slotCount = 0;
    auto includeSlot = [&](hir::HirSlotId slot) {
        if (slot == hir::kInvalidHirSlot || slot >= 0xFFFFU) {
            state.result->ok      = false;
            state.result->message = "HIR slot index exceeds v2 register limit";
            return false;
        }
        slotCount = std::max(slotCount, static_cast<std::size_t>(slot) + 1U);
        return true;
    };
    for (const auto slot : state.hirFn->param_slots)
        if (!includeSlot(slot))
            return false;
    for (const auto &block : state.hirFn->blocks) {
        for (const auto instId : block.insts) {
            const auto &expr = state.hir.getExpr(instId);
            if (const auto *alloca = std::get_if<hir::HirSlotAlloca>(&expr)) {
                if (!includeSlot(alloca->slot))
                    return false;
            } else if (const auto *store = std::get_if<hir::HirSlotStore>(&expr)) {
                if (!includeSlot(store->slot))
                    return false;
            } else if (const auto *load = std::get_if<hir::HirSlotLoad>(&expr)) {
                if (!includeSlot(load->slot))
                    return false;
            } else if (const auto *addr = std::get_if<hir::HirSlotAddr>(&expr)) {
                if (!includeSlot(addr->slot))
                    return false;
            }
        }
    }
    state.slotRegs.resize(slotCount, kUnassignedReg);
    state.slotWidths.resize(slotCount, 0U);

    for (const auto &block : state.hirFn->blocks) {
        for (const auto instId : block.insts) {
            const auto *alloca = std::get_if<hir::HirSlotAlloca>(&state.hir.getExpr(instId));
            if (alloca == nullptr || alloca->slot >= state.slotWidths.size())
                continue;
            const auto width = static_cast<std::uint8_t>(
                valueRegisterWidth(state.types, alloca->type));
            if (state.slotWidths[alloca->slot] != 0U &&
                state.slotWidths[alloca->slot] != width) {
                state.result->ok      = false;
                state.result->message = "inconsistent HIR slot widths in v2 lowering";
                return false;
            }
            state.slotWidths[alloca->slot] = width;
        }
    }
    for (std::size_t i = 0; i < state.hirFn->param_slots.size() &&
                            i < state.hirFn->params.size();
         ++i) {
        const auto slot = state.hirFn->param_slots[i];
        if (slot >= state.slotRegs.size())
            continue;
        const auto width = valueRegisterWidth(state.types, state.hirFn->params[i]);
        if (state.slotWidths[slot] == 0U)
            state.slotWidths[slot] = static_cast<std::uint8_t>(width);
        if (state.slotWidths[slot] != width) {
            state.result->ok      = false;
            state.result->message = "parameter slot width disagrees with its v2 ABI";
            return false;
        }
        state.slotRegs[slot] = state.paramRegs[i];
    }

    for (std::size_t slot = 0; slot < state.slotRegs.size(); ++slot) {
        if (state.slotWidths[slot] == 0U)
            state.slotWidths[slot] = 1U;
        if (state.slotRegs[slot] != kUnassignedReg)
            continue;
        const auto width = static_cast<std::size_t>(state.slotWidths[slot]);
        if (nextRegister + width > 0xFFFFU) {
            state.result->ok      = false;
            state.result->message = "v2 function local registers exceed 65535";
            return false;
        }
        state.slotRegs[slot] = u16(nextRegister, state.result);
        nextRegister += width;
    }

    state.fn->regCount = u16(std::max<std::size_t>(nextRegister, 1U), state.result);
    if (!state.result->ok)
        return false;
    while (state.fn->regTypes.size() < state.fn->regCount)
        state.fn->regTypes.push(ValueType::I64);
    return true;
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
        fn.returnType = valueTypeOf(types, hirFn.return_type);

        LowerState state{arena, hir, interner, types, out, &hirFn, &fn,
                         memory::DynArray<std::uint16_t>(arena),
                         memory::DynArray<std::uint16_t>(arena),
                         memory::DynArray<std::uint16_t>(arena),
                         memory::DynArray<std::uint8_t>(arena), &result};
        state.regs.resize(hir.exprCount(), kUnassignedReg);
        if (!prepareFunctionRegisters(state))
            return result;

        struct PendingTarget {
            std::size_t pc = 0;
            hir::HirDeclId block = hir::kInvalidHirExpr;
            bool falseBranch = false;
        };
        std::vector<std::size_t> blockPcs(hirFn.blocks.size(), fn.body.size());
        std::vector<PendingTarget> pendingTargets;

        for (std::size_t blockIndex = 0; blockIndex < hirFn.blocks.size(); ++blockIndex) {
            const auto &block = hirFn.blocks[blockIndex];
            blockPcs[blockIndex] = fn.body.size();
            for (const auto instId : block.insts) {
                const auto &expr = hir.getExpr(instId);
                if (std::holds_alternative<hir::HirSlotAlloca>(expr)) {
                    continue;
                } else if (std::holds_alternative<hir::HirSlotStore>(expr)) {
                    const auto &store = std::get<hir::HirSlotStore>(expr);
                    const auto destination = slotRegister(state, store.slot);
                    const auto value = lowerOperand(state, store.value);
                    if (destination == kUnassignedReg) {
                        result.ok      = false;
                        result.message = "invalid HIR slot store in v2 lowering";
                        return result;
                    }
                    if (value == kUnassignedReg) {
                        if (!result.ok)
                            return result;
                        continue;
                    }
                    const bool isSliceSlot = state.slotWidths[store.slot] == 2U;
                    if (isSliceSlot &&
                        (static_cast<std::size_t>(destination) + 1U >= fn.regCount ||
                         static_cast<std::size_t>(value) + 1U >= fn.regCount)) {
                        result.ok      = false;
                        result.message = "slice HIR slot store exceeds v2 registers";
                        return result;
                    }
                    if (destination != value)
                        emit(state, Instr::simple(Op::Move, destination, value));
                    if (isSliceSlot) {
                        emit(state, Instr::simple(
                                        Op::Move, static_cast<std::uint16_t>(destination + 1U),
                                        static_cast<std::uint16_t>(value + 1U)));
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
                    result.message = "unsupported return value in v2 lowering for " +
                                     std::string(linkage);
                    return result;
                }
                emit(state, Instr{Op::Ret, static_cast<std::uint16_t>(value), 0, 0, 0});
            } else if (const auto *jump = std::get_if<hir::HirJump>(&terminator)) {
                if (jump->target >= hirFn.blocks.size()) {
                    result.ok      = false;
                    result.message = "invalid HIR jump target in v2 lowering";
                    return result;
                }
                pendingTargets.push_back({fn.body.size(), jump->target, false});
                emit(state, Instr{Op::Jump, 0, 0, 0, 0});
            } else if (const auto *branch = std::get_if<hir::HirBranch>(&terminator)) {
                const auto cond = lowerOperand(state, branch->cond);
                if (cond == kUnassignedReg || branch->then_block >= hirFn.blocks.size() ||
                    branch->else_block >= hirFn.blocks.size()) {
                    result.ok      = false;
                    result.message = "invalid HIR branch in v2 lowering";
                    return result;
                }
                pendingTargets.push_back({fn.body.size(), branch->then_block, false});
                pendingTargets.push_back({fn.body.size(), branch->else_block, true});
                emit(state, Instr{Op::Branch2, 0, cond, 0, 0, 0});
            }
        }

        for (const auto &pending : pendingTargets) {
            if (pending.block >= blockPcs.size() || blockPcs[pending.block] >= fn.body.size()) {
                result.ok      = false;
                result.message = "unresolved HIR block target in v2 lowering";
                return result;
            }
            auto &instruction = fn.body[pending.pc];
            if (pending.falseBranch)
                instruction.d = u16(blockPcs[pending.block], &result);
            else
                instruction.imm = u16(blockPcs[pending.block], &result);
        }
    }

    return result;
}

} // namespace zith::vm
