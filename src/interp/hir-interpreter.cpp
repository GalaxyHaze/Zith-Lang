#include "hir-interpreter.hpp"

#include "hir/hir-expr.hpp"
#include "symbols/symbol-id.hpp"
#include "types/type-kind.hpp"

#include <cstdio>

namespace zith::interp {
namespace {

std::string_view sourceName(std::string_view linkage) {
    auto paren = linkage.find('(');
    if (paren != std::string_view::npos)
        linkage = linkage.substr(0, paren);
    auto dot = linkage.rfind('.');
    if (dot != std::string_view::npos)
        linkage = linkage.substr(dot + 1);
    return linkage;
}

} // namespace

HirInterpreter::HirInterpreter(const hir::HirModule &module, const memory::StringInterner &interner,
                               const types::TypeIntern &types)
    : module_(module), interner_(interner), types_(types) {}

bool HirInterpreter::findFunction(std::string_view name, const hir::HirFunction *&fn) const {
    for (size_t i = 0; i < module_.getFnCount(); ++i) {
        const auto &candidate = module_.getFn(i);
        const auto linkage    = interner_.lookup(candidate.name);
        if (sourceName(linkage) == name) {
            fn = &candidate;
            return true;
        }
    }
    return false;
}

const hir::HirFunction *HirInterpreter::findMain() const {
    const hir::HirFunction *main = nullptr;
    return findFunction("main", main) ? main : nullptr;
}

HirInterpResult HirInterpreter::runMain() {
    HirInterpResult result;
    const auto *main = findMain();
    if (main == nullptr) {
        result.status  = HirInterpStatus::MissingMain;
        result.message = "interpreter requires a definitive main function";
        return result;
    }

    memory::DynArray<Value> args(frameArena_);
    int64_t ret = 0;
    if (!runFunction(*main, args, ret, result)) {
        if (result.status == HirInterpStatus::Ok)
            result.status = HirInterpStatus::InternalError;
        result.exitCode = 1;
        return result;
    }

    result.status   = HirInterpStatus::Ok;
    result.exitCode = ret;
    result.output   = std::move(output_);
    return result;
}

HirInterpreter::Value HirInterpreter::eval(hir::HirExprId id, Frame &frame,
                                           HirInterpResult &result) {
    const auto &expr = module_.getExpr(id);
    return std::visit(
        [&](const auto &node) -> Value {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, hir::HirLiteral>) {
                Value value;
                switch (types_.kindOf(node.type)) {
                case types::TypeKind::Int:
                    value.kind = Value::Kind::Int;
                    value.i    = node.i;
                    break;
                case types::TypeKind::Bool:
                    value.kind = Value::Kind::Int;
                    value.i    = node.b ? 1 : 0;
                    break;
                case types::TypeKind::Char:
                    value.kind = Value::Kind::Int;
                    value.i    = node.i;
                    break;
                case types::TypeKind::Ptr:
                    value.kind = Value::Kind::String;
                    value.text = interner_.lookup(node.str_val);
                    break;
                default:
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    break;
                }
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirBinary>) {
                const auto lhs = eval(node.lhs, frame, result);
                const auto rhs = eval(node.rhs, frame, result);
                Value value;
                value.kind = Value::Kind::Int;
                if (lhs.kind != Value::Kind::Int || rhs.kind != Value::Kind::Int) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    return value;
                }
                switch (node.op) {
                case hir::HirBinaryOp::Add:
                    value.i = lhs.i + rhs.i;
                    break;
                case hir::HirBinaryOp::Sub:
                    value.i = lhs.i - rhs.i;
                    break;
                case hir::HirBinaryOp::Mul:
                    value.i = lhs.i * rhs.i;
                    break;
                case hir::HirBinaryOp::Div:
                    value.i = lhs.i / rhs.i;
                    break;
                case hir::HirBinaryOp::Eq:
                    value.i = lhs.i == rhs.i;
                    break;
                case hir::HirBinaryOp::Ne:
                    value.i = lhs.i != rhs.i;
                    break;
                default:
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    value.kind      = Value::Kind::Invalid;
                    break;
                }
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirUnary>) {
                const auto operand = eval(node.operand, frame, result);
                Value value;
                value.kind = Value::Kind::Int;
                switch (node.op) {
                case hir::HirUnaryOp::Neg:
                    value.i = -operand.i;
                    break;
                case hir::HirUnaryOp::Not:
                    value.i = operand.i == 0;
                    break;
                case hir::HirUnaryOp::BitNot:
                    value.i = ~operand.i;
                    break;
                default:
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    value.kind      = Value::Kind::Invalid;
                    break;
                }
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirSlotAlloca>) {
                frame.slots.resize(static_cast<size_t>(node.slot) + 1, 0);
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirSlotLoad>) {
                if (node.slot >= frame.slots.size()) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                Value value;
                value.kind = Value::Kind::Int;
                value.i    = frame.slots[node.slot];
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirSlotStore>) {
                const auto value = eval(node.value, frame, result);
                if (value.kind != Value::Kind::Int) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    return {};
                }
                if (node.slot >= frame.slots.size())
                    frame.slots.resize(static_cast<size_t>(node.slot) + 1, 0);
                frame.slots[node.slot] = value.i;
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirVar>) {
                if (frame.fn != nullptr) {
                    for (size_t index = 0; index < frame.fn->param_names.size(); ++index) {
                        if (frame.fn->param_names[index] == node.name) {
                            if (index < frame.regs.size())
                                return frame.regs[index];
                            break;
                        }
                    }
                }
                result.status   = HirInterpStatus::InternalError;
                result.exitCode = 1;
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirCall>) {
                const hir::HirFunction *callee = nullptr;
                if (node.resolved_fn != symbols::kInvalidSym) {
                    for (size_t index = 0; index < module_.getFnCount(); ++index) {
                        if (module_.getFn(index).sym_id == node.resolved_fn) {
                            callee = &module_.getFn(index);
                            break;
                        }
                    }
                }
                if (callee == nullptr) {
                    if (const auto *var = std::get_if<hir::HirVar>(&module_.getExpr(node.callee))) {
                        const auto name = interner_.lookup(var->name);
                        findFunction(name, callee);
                    }
                }
                if (callee == nullptr) {
                    result.status   = HirInterpStatus::MissingExtern;
                    result.exitCode = 1;
                    return {};
                }

                memory::DynArray<Value> args(frameArena_);
                for (auto arg_id : node.args)
                    args.push(eval(arg_id, frame, result));

                int64_t ret = 0;
                if (callee->blocks.empty()) {
                    if (!runExtern(*callee, args, ret)) {
                        result.status   = HirInterpStatus::MissingExtern;
                        result.exitCode = 1;
                        return {};
                    }
                } else if (!runFunction(*callee, args, ret, result)) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }

                Value value;
                value.kind = Value::Kind::Int;
                value.i    = ret;
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirRet>) {
                const auto value = eval(node.value, frame, result);
                if (value.kind != Value::Kind::Int) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    return {};
                }
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirBranch>) {
                const auto cond = eval(node.cond, frame, result);
                if (cond.kind != Value::Kind::Int) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    return {};
                }
                frame.block = cond.i != 0 ? node.then_block : node.else_block;
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirJump>) {
                frame.block = node.target;
                return {};
            } else {
                result.status   = HirInterpStatus::UnsupportedExpr;
                result.exitCode = 1;
                return {};
            }
        },
        expr);
}

bool HirInterpreter::runFunction(const hir::HirFunction &fn, memory::DynArray<Value> &args,
                                 int64_t &returnValue, HirInterpResult &result) {
    Frame frame(&fn, frameArena_);
    frame.regs.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i)
        frame.regs.push(args[i]);

    size_t block = 0;
    bool stopped = false;
    while (block < fn.blocks.size()) {
        const auto &hir_block = fn.blocks[block];
        for (auto inst : hir_block.insts) {
            eval(inst, frame, result);
            if (result.status != HirInterpStatus::Ok)
                return false;
        }

        if (hir_block.terminator == hir::kInvalidHirExpr) {
            block++;
            continue;
        }

        const auto &terminator = module_.getExpr(hir_block.terminator);
        const auto *ret_node   = std::get_if<hir::HirRet>(&terminator);
        Value value;
        if (ret_node != nullptr && ret_node->value != hir::kInvalidHirExpr)
            value = eval(hir_block.terminator, frame, result);
        if (result.status != HirInterpStatus::Ok)
            return false;

        if (ret_node != nullptr) {
            returnValue = value.i;
            stopped     = true;
            break;
        }
        if (const auto *jump = std::get_if<hir::HirJump>(&terminator)) {
            block = jump->target;
            continue;
        }
        if (std::get_if<hir::HirBranch>(&terminator) != nullptr) {
            if (frame.block != static_cast<size_t>(-1)) {
                block = frame.block;
                continue;
            }
        }

        result.status   = HirInterpStatus::UnsupportedExpr;
        result.exitCode = 1;
        return false;
    }

    if (!stopped) {
        result.status   = HirInterpStatus::InternalError;
        result.exitCode = 1;
        return false;
    }
    return true;
}

bool HirInterpreter::runExtern(const hir::HirFunction &fn, memory::DynArray<Value> &args,
                               int64_t &returnValue) {
    const auto name = sourceName(interner_.lookup(fn.name));
    if (name == "puts" && args.size() > 0) {
        const auto &text = args[0].text;
        output_.append(text.data(), text.size());
        output_.push_back('\n');
        returnValue = static_cast<int64_t>(text.size());
        return true;
    }
    if (name == "putchar" && args.size() > 0) {
        output_.push_back(static_cast<char>(args[0].i));
        returnValue = args[0].i;
        return true;
    }
    return false;
}

} // namespace zith::interp
