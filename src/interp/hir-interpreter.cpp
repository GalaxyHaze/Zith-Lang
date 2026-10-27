#include "hir-interpreter.hpp"

#include "hir/hir-expr.hpp"
#include "symbols/symbol-id.hpp"
#include "types/type-kind.hpp"

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <limits>
#include <type_traits>

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
    : module_(module), interner_(interner), types_(types) {
    const auto visitSlot = [this](const hir::HirExpr &expr) {
        std::visit(
            [this](const auto &node) {
                using T = std::decay_t<decltype(node)>;
                hir::HirSlotId slot = hir::kInvalidHirSlot;
                if constexpr (std::is_same_v<T, hir::HirSlotAlloca> ||
                              std::is_same_v<T, hir::HirSlotLoad> ||
                              std::is_same_v<T, hir::HirSlotAddr>)
                    slot = node.slot;
                else if constexpr (std::is_same_v<T, hir::HirSlotStore>)
                    slot = node.slot;
                if (slot != hir::kInvalidHirSlot)
                    slotCount_ = std::max(slotCount_, static_cast<size_t>(slot) + 1U);
            },
            expr);
    };
    for (size_t index = 0; index < module_.exprCount(); ++index)
        visitSlot(module_.getExpr(static_cast<hir::HirExprId>(index)));
    slotCount_ = std::max(slotCount_, module_.attrs().slotCount());
}

uint32_t HirInterpreter::addObject(types::TypeId type) {
    if (objects_.size() >= static_cast<size_t>(kInvalidObject))
        return kInvalidObject;
    objects_.emplace_back(frameArena_, type);
    return static_cast<uint32_t>(objects_.size() - 1U);
}

HirInterpreter::Value HirInterpreter::pointerToCell(Value &cell) const {
    Value pointer;
    pointer.kind = Value::Kind::Ptr;
    pointer.cell = &cell;
    return pointer;
}

HirInterpreter::Value HirInterpreter::pointerToObject(uint32_t object) const {
    Value pointer;
    pointer.kind   = Value::Kind::Ptr;
    pointer.object = object;
    return pointer;
}

bool HirInterpreter::isAggregateType(types::TypeId type) const {
    switch (types_.kindOf(type)) {
    case types::TypeKind::Array:
    case types::TypeKind::Pack:
    case types::TypeKind::Struct:
    case types::TypeKind::Union:
    case types::TypeKind::Optional:
    case types::TypeKind::Sum:
        return true;
    default:
        return false;
    }
}

HirInterpreter::Value HirInterpreter::cloneAggregate(const Value &value) {
    if (value.kind != Value::Kind::Aggregate || value.object >= objects_.size())
        return value;

    const auto &source = objects_[value.object];
    const auto type    = source.type;
    const bool alive   = source.alive;
    std::vector<Value> fields;
    fields.reserve(source.fields.size());
    for (const auto &field : source.fields)
        fields.push_back(field);
    const auto bytes = source.bytes;
    if (!alive)
        return {};

    const auto object = addObject(type);
    if (object == kInvalidObject)
        return {};
    auto &copy = objects_[object];
    copy.fields.reserve(fields.size());
    for (const auto &field : fields)
        copy.fields.push(field);
    copy.bytes = bytes;
    Value clone;
    clone.kind   = Value::Kind::Aggregate;
    clone.object = object;
    return clone;
}

bool HirInterpreter::readByte(const Value &pointer, size_t index, char &value) const {
    if (pointer.kind == Value::Kind::String) {
        if (index >= pointer.text.size())
            return false;
        value = pointer.text[index];
        return true;
    }
    if (pointer.kind == Value::Kind::Slice) {
        if (index >= pointer.length)
            return false;
        if (pointer.cell != nullptr) {
            const auto &backing = *pointer.cell;
            if (backing.kind == Value::Kind::Int) {
                if (index != 0U || pointer.offset != 0U)
                    return false;
                value = static_cast<char>(backing.i);
                return true;
            }
            if (backing.kind == Value::Kind::Ptr || backing.kind == Value::Kind::Slice) {
                auto adjusted = backing;
                adjusted.offset += pointer.offset;
                return readByte(adjusted, index, value);
            }
            if (backing.kind == Value::Kind::String) {
                const auto offset = pointer.offset + index;
                if (offset >= backing.text.size())
                    return false;
                value = backing.text[offset];
                return true;
            }
            if (backing.kind == Value::Kind::Aggregate &&
                backing.object < objects_.size()) {
                const auto &fields = objects_[backing.object].fields;
                if (pointer.offset >= fields.size() ||
                    index >= fields.size() - pointer.offset)
                    return false;
                const auto &element = fields[pointer.offset + index];
                if (element.kind != Value::Kind::Int)
                    return false;
                value = static_cast<char>(element.i);
                return true;
            }
            return false;
        }
        if (pointer.object == kInvalidObject) {
            const auto offset = pointer.offset + index;
            if (offset >= pointer.text.size())
                return false;
            value = pointer.text[offset];
            return true;
        }
        if (pointer.object >= objects_.size())
            return false;
        const auto &object = objects_[pointer.object];
        const auto offset  = pointer.offset + index;
        if (!object.alive)
            return false;
        if (offset < object.bytes.size()) {
            value = object.bytes[offset];
            return true;
        }
        if (offset < object.fields.size() &&
            object.fields[offset].kind == Value::Kind::Int) {
            value = static_cast<char>(object.fields[offset].i);
            return true;
        }
        return false;
    }
    if (pointer.kind != Value::Kind::Ptr)
        return false;
    if (pointer.cell != nullptr) {
        const auto &backing = *pointer.cell;
        if (backing.kind == Value::Kind::Int) {
            if (index != 0U || pointer.offset != 0U)
                return false;
            value = static_cast<char>(backing.i);
            return true;
        }
        if (backing.kind == Value::Kind::Ptr || backing.kind == Value::Kind::Slice) {
            auto adjusted = backing;
            adjusted.offset += pointer.offset;
            return readByte(adjusted, index, value);
        }
        if (backing.kind == Value::Kind::String) {
            const auto offset = pointer.offset + index;
            if (offset >= backing.text.size())
                return false;
            value = backing.text[offset];
            return true;
        }
        return false;
    }
    if (pointer.text.data() != nullptr) {
        const auto offset = pointer.offset + index;
        if (offset >= pointer.text.size())
            return false;
        value = pointer.text[offset];
        return true;
    }
    if (pointer.object == kInvalidObject || pointer.object >= objects_.size())
        return false;
    const auto &object = objects_[pointer.object];
    const auto offset  = pointer.offset + index;
    if (!object.alive)
        return false;
    if (offset < object.bytes.size()) {
        value = object.bytes[offset];
        return true;
    }
    if (offset < object.fields.size() && object.fields[offset].kind == Value::Kind::Int) {
        value = static_cast<char>(object.fields[offset].i);
        return true;
    }
    return false;
}

bool HirInterpreter::writeByte(const Value &pointer, size_t index, char value) {
    if (pointer.kind == Value::Kind::Ptr && pointer.cell != nullptr) {
        if (pointer.cell->kind == Value::Kind::Int) {
            if (index != 0U || pointer.offset != 0U)
                return false;
            pointer.cell->i = static_cast<unsigned char>(value);
            return true;
        }
        if (pointer.cell->kind == Value::Kind::Ptr ||
            pointer.cell->kind == Value::Kind::Slice) {
            auto backing = *pointer.cell;
            backing.offset += pointer.offset;
            return writeByte(backing, index, value);
        }
        return false;
    }
    if (pointer.kind != Value::Kind::Ptr || pointer.object == kInvalidObject ||
        pointer.object >= objects_.size())
        return false;
    auto &object = objects_[pointer.object];
    const auto offset = pointer.offset + index;
    if (!object.alive || offset >= object.bytes.size())
        return false;
    object.bytes[offset] = value;
    return true;
}

HirInterpreter::Value HirInterpreter::readPointer(const Value &pointer,
                                                  HirInterpResult &result) {
    if (pointer.kind != Value::Kind::Ptr) {
        result.status   = HirInterpStatus::UnsupportedExpr;
        result.exitCode = 1;
        result.message = "HIR interpreter expected a pointer value";
        return {};
    }
    if (pointer.cell != nullptr)
        return *pointer.cell;
    if (pointer.text.data() != nullptr) {
        char byte = 0;
        if (!readByte(pointer, 0, byte))
            return {};
        Value value;
        value.kind = Value::Kind::Int;
        value.i    = static_cast<unsigned char>(byte);
        return value;
    }
    if (pointer.object == kInvalidObject || pointer.object >= objects_.size()) {
        Value value;
        value.kind = Value::Kind::Int;
        return value;
    }

    const auto &object = objects_[pointer.object];
    if (!object.alive) {
        result.status   = HirInterpStatus::InternalError;
        result.exitCode = 1;
        result.message = "HIR interpreter dereferenced released memory";
        return {};
    }
    if (!object.bytes.empty()) {
        char byte = 0;
        if (!readByte(pointer, 0, byte))
            return {};
        Value value;
        value.kind = Value::Kind::Int;
        value.i    = static_cast<unsigned char>(byte);
        return value;
    }
    if (isAggregateType(object.type)) {
        Value value;
        value.kind   = Value::Kind::Aggregate;
        value.object = pointer.object;
        return value;
    }
    if (!object.fields.empty())
        return object.fields[0];
    return {};
}

bool HirInterpreter::writePointer(const Value &pointer, Value value, HirInterpResult &result) {
    if (pointer.kind != Value::Kind::Ptr) {
        result.status   = HirInterpStatus::UnsupportedExpr;
        result.exitCode = 1;
        result.message = "HIR interpreter expected an addressable value";
        return false;
    }
    if (pointer.cell != nullptr) {
        if (value.kind == Value::Kind::Aggregate)
            value = cloneAggregate(value);
        *pointer.cell = value;
        return true;
    }
    if (value.kind != Value::Kind::Int ||
        !writeByte(pointer, 0, static_cast<char>(value.i))) {
        result.status   = HirInterpStatus::UnsupportedExpr;
        result.exitCode = 1;
        result.message = "HIR interpreter cannot store through this pointer";
        return false;
    }
    return true;
}

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
    Value ret;
    if (!runFunction(*main, args, ret, result)) {
        if (result.status == HirInterpStatus::Ok)
            result.status = HirInterpStatus::InternalError;
        result.exitCode = 1;
        return result;
    }

    result.status   = HirInterpStatus::Ok;
    result.exitCode = ret.kind == Value::Kind::Int ? ret.i : 0;
    result.output   = output_.take();
    return result;
}

HirInterpreter::Value HirInterpreter::addressOf(hir::HirExprId id, Frame &frame,
                                                HirInterpResult &result) {
    if (id == hir::kInvalidHirExpr || id >= module_.exprCount())
        return {};
    const auto &expr = module_.getExpr(id);
    return std::visit(
        [&](const auto &node) -> Value {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, hir::HirSlotAddr>) {
                if (node.slot >= frame.slots.size()) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                return pointerToCell(frame.slots[node.slot]);
            } else if constexpr (std::is_same_v<T, hir::HirVar>) {
                if (frame.fn != nullptr) {
                    for (size_t index = 0; index < frame.fn->param_names.size(); ++index) {
                        if (frame.fn->param_names[index] == node.name &&
                            index < frame.regs.size())
                            return pointerToCell(frame.regs[index]);
                    }
                }
                result.status   = HirInterpStatus::InternalError;
                result.exitCode = 1;
                result.message  = "HIR interpreter cannot take the address of this variable";
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirField>) {
                auto base = eval(node.object, frame, result);
                if (base.kind == Value::Kind::Ptr)
                    base = readPointer(base, result);
                if (base.kind != Value::Kind::Aggregate || base.object >= objects_.size() ||
                    node.index >= objects_[base.object].fields.size()) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message  = "HIR interpreter cannot address this aggregate field";
                    return {};
                }
                return pointerToCell(objects_[base.object].fields[node.index]);
            } else if constexpr (std::is_same_v<T, hir::HirIndex>) {
                const auto object = eval(node.object, frame, result);
                const auto index  = eval(node.index, frame, result);
                if (index.kind != Value::Kind::Int || index.i < 0) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    return {};
                }
                if (object.kind == Value::Kind::Aggregate && object.object < objects_.size()) {
                    auto &fields = objects_[object.object].fields;
                    if (static_cast<uint64_t>(index.i) >= fields.size()) {
                        result.status   = HirInterpStatus::UnsupportedExpr;
                        result.exitCode = 1;
                        result.message  = "HIR interpreter array index is out of bounds";
                        return {};
                    }
                    return pointerToCell(fields[static_cast<size_t>(index.i)]);
                }
                if (object.kind == Value::Kind::Slice &&
                    static_cast<uint64_t>(index.i) < object.length) {
                    Value pointer;
                    pointer.kind   = Value::Kind::Ptr;
                    pointer.object = object.object;
                    pointer.text   = object.text;
                    pointer.offset = object.offset + static_cast<size_t>(index.i);
                    return pointer;
                }
                if (object.kind == Value::Kind::Ptr) {
                    Value pointer = object;
                    pointer.offset += static_cast<size_t>(index.i);
                    return pointer;
                }
                result.status   = HirInterpStatus::UnsupportedExpr;
                result.exitCode = 1;
                result.message  = "HIR interpreter cannot address this indexed value";
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirUnary>) {
                if (node.op == hir::HirUnaryOp::Deref)
                    return eval(node.operand, frame, result);
                if (node.op == hir::HirUnaryOp::Ref)
                    return addressOf(node.operand, frame, result);
                result.status   = HirInterpStatus::UnsupportedExpr;
                result.exitCode = 1;
                return {};
            } else {
                auto value = eval(id, frame, result);
                if (value.kind == Value::Kind::Aggregate)
                    return pointerToObject(value.object);
                const auto object = addObject(types_.kindOf(types::kInvalidType) ==
                                                      types::TypeKind::Invalid
                                                  ? types::kInvalidType
                                                  : types::kInvalidType);
                if (object == kInvalidObject) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                objects_[object].fields.push(value);
                return pointerToObject(object);
            }
        },
        expr);
}

HirInterpreter::Value HirInterpreter::eval(hir::HirExprId id, Frame &frame,
                                           HirInterpResult &result) {
    const auto &expr = module_.getExpr(id);
    auto evaluated = std::visit(
        [&](const auto &node) -> Value {
            using T = std::decay_t<decltype(node)>;
            if constexpr (std::is_same_v<T, hir::HirLiteral>) {
                Value value;
                switch (types_.kindOf(node.type)) {
                case types::TypeKind::Int:
                case types::TypeKind::Enum:
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
                case types::TypeKind::Float:
                    value.kind = Value::Kind::Float;
                    value.f    = node.f;
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
                if (result.status != HirInterpStatus::Ok)
                    return {};
                const auto rhs = eval(node.rhs, frame, result);
                if (result.status != HirInterpStatus::Ok)
                    return {};
                Value value;
                value.kind = Value::Kind::Int;
                if ((node.op == hir::HirBinaryOp::Eq || node.op == hir::HirBinaryOp::Ne) &&
                    ((lhs.kind == Value::Kind::Ptr && rhs.kind == Value::Kind::Int &&
                      rhs.i == 0) ||
                     (rhs.kind == Value::Kind::Ptr && lhs.kind == Value::Kind::Int &&
                      lhs.i == 0))) {
                    const auto &pointer =
                        lhs.kind == Value::Kind::Ptr ? lhs : rhs;
                    const bool isNull =
                        pointer.object == kInvalidObject && pointer.cell == nullptr &&
                        pointer.text.data() == nullptr;
                    value.i = node.op == hir::HirBinaryOp::Eq ? isNull : !isNull;
                    return value;
                }
                if (lhs.kind != Value::Kind::Int || rhs.kind != Value::Kind::Int) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message =
                        "HIR interpreter cannot evaluate binary expression " +
                        std::to_string(id) + " (operation " +
                        std::to_string(static_cast<unsigned>(node.op)) + ", operand expressions " +
                        std::to_string(node.lhs) + " and " + std::to_string(node.rhs) +
                        " (HIR kinds " +
                        std::to_string(static_cast<unsigned>(
                            hir::exprKind(module_.getExpr(node.lhs)))) +
                        " and " +
                        std::to_string(static_cast<unsigned>(
                            hir::exprKind(module_.getExpr(node.rhs)))) +
                        ")" +
                        ", operand kinds " + std::to_string(static_cast<unsigned>(lhs.kind)) +
                        " and " + std::to_string(static_cast<unsigned>(rhs.kind)) + ", type " +
                        std::to_string(node.operand_type) + ", function " +
                        (frame.fn != nullptr ? std::string(interner_.lookup(frame.fn->name))
                                             : std::string("<unknown>")) +
                        ")";
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
                case hir::HirBinaryOp::Rem:
                    value.i = lhs.i % rhs.i;
                    break;
                case hir::HirBinaryOp::Eq:
                    value.i = lhs.i == rhs.i;
                    break;
                case hir::HirBinaryOp::Ne:
                    value.i = lhs.i != rhs.i;
                    break;
                case hir::HirBinaryOp::Lt:
                    value.i = lhs.i < rhs.i;
                    break;
                case hir::HirBinaryOp::Le:
                    value.i = lhs.i <= rhs.i;
                    break;
                case hir::HirBinaryOp::Gt:
                    value.i = lhs.i > rhs.i;
                    break;
                case hir::HirBinaryOp::Ge:
                    value.i = lhs.i >= rhs.i;
                    break;
                case hir::HirBinaryOp::And:
                    value.i = lhs.i & rhs.i;
                    break;
                case hir::HirBinaryOp::Or:
                    value.i = lhs.i | rhs.i;
                    break;
                case hir::HirBinaryOp::Xor:
                    value.i = lhs.i ^ rhs.i;
                    break;
                case hir::HirBinaryOp::Shl:
                    value.i = lhs.i << rhs.i;
                    break;
                case hir::HirBinaryOp::Shr:
                    value.i = lhs.i >> rhs.i;
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
                switch (node.op) {
                case hir::HirUnaryOp::Neg:
                    value.kind = operand.kind;
                    value.i = -operand.i;
                    break;
                case hir::HirUnaryOp::Not:
                    value.kind = Value::Kind::Int;
                    value.i = operand.i == 0;
                    break;
                case hir::HirUnaryOp::BitNot:
                    value.kind = Value::Kind::Int;
                    value.i = ~operand.i;
                    break;
                case hir::HirUnaryOp::Ref:
                    return addressOf(node.operand, frame, result);
                case hir::HirUnaryOp::Deref:
                    return readPointer(operand, result);
                }
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirStructLiteral>) {
                const auto object = addObject(node.type);
                if (object == kInvalidObject) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                objects_[object].fields.resize(
                    std::max(node.values.size(), types_.memberCount(node.type)));
                for (size_t index = 0; index < node.values.size(); ++index) {
                    if (node.values[index] != hir::kInvalidHirExpr)
                        objects_[object].fields[index] = eval(node.values[index], frame, result);
                    if (result.status != HirInterpStatus::Ok)
                        return {};
                }
                Value value;
                value.kind   = Value::Kind::Aggregate;
                value.object = object;
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirArrayLiteral>) {
                const auto object = addObject(node.type);
                if (object == kInvalidObject) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                objects_[object].fields.resize(node.elements.size());
                for (size_t index = 0; index < node.elements.size(); ++index) {
                    objects_[object].fields[index] = eval(node.elements[index], frame, result);
                    if (result.status != HirInterpStatus::Ok)
                        return {};
                }
                Value value;
                value.kind   = Value::Kind::Aggregate;
                value.object = object;
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirField>) {
                return readPointer(addressOf(id, frame, result), result);
            } else if constexpr (std::is_same_v<T, hir::HirIndex>) {
                const auto object = eval(node.object, frame, result);
                const auto index  = eval(node.index, frame, result);
                if (result.status != HirInterpStatus::Ok)
                    return {};
                if (index.kind != Value::Kind::Int || index.i < 0) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message =
                        "HIR interpreter cannot evaluate index expression " +
                        std::to_string(node.index) + " (HIR kind " +
                        std::to_string(static_cast<unsigned>(
                            hir::exprKind(module_.getExpr(node.index)))) +
                        ", value kind " +
                        std::to_string(static_cast<unsigned>(index.kind)) + ")";
                    return {};
                }
                const auto itemIndex = static_cast<size_t>(index.i);
                const auto invalidElement = [&]() {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message =
                        "HIR interpreter indexed an invalid element (object expression " +
                        std::to_string(node.object) + ", object kind " +
                        std::to_string(static_cast<unsigned>(object.kind)) + ", object " +
                        std::to_string(object.object) + ", cell " +
                        std::to_string(object.cell != nullptr ? 1 : 0) + ", index " +
                        std::to_string(itemIndex) + ")";
                    return Value{};
                };
                if (object.kind == Value::Kind::Aggregate && object.object < objects_.size()) {
                    const auto &fields = objects_[object.object].fields;
                    if (itemIndex < fields.size()) {
                        if (fields[itemIndex].kind == Value::Kind::Invalid)
                            return invalidElement();
                        return fields[itemIndex];
                    }
                } else if (object.kind == Value::Kind::Slice) {
                    if (itemIndex >= object.length) {
                        result.status   = HirInterpStatus::UnsupportedExpr;
                        result.exitCode = 1;
                        result.message =
                            "HIR interpreter slice index is out of bounds (object expression " +
                            std::to_string(node.object) + ", index " +
                            std::to_string(itemIndex) + ", length " +
                            std::to_string(object.length) + ")";
                        return {};
                    }
                    if (object.cell != nullptr) {
                        const auto &backing = *object.cell;
                        if (backing.kind == Value::Kind::Aggregate &&
                            backing.object < objects_.size()) {
                            const auto &fields = objects_[backing.object].fields;
                            if (object.offset < fields.size() &&
                                itemIndex < fields.size() - object.offset) {
                                const auto &element = fields[object.offset + itemIndex];
                                if (element.kind == Value::Kind::Invalid)
                                    return invalidElement();
                                return element;
                            }
                        }
                        char byte = 0;
                        if (readByte(object, itemIndex, byte)) {
                            Value value;
                            value.kind = Value::Kind::Int;
                            value.i    = static_cast<unsigned char>(byte);
                            return value;
                        }
                    }
                    if (object.object == kInvalidObject) {
                        const auto offset = object.offset + itemIndex;
                        if (offset < object.text.size()) {
                            Value value;
                            value.kind = Value::Kind::Int;
                            value.i = static_cast<unsigned char>(object.text[offset]);
                            return value;
                        }
                    } else if (object.object < objects_.size()) {
                        const auto &storage = objects_[object.object];
                        const auto offset   = object.offset + itemIndex;
                        if (offset < storage.bytes.size()) {
                            Value value;
                            value.kind = Value::Kind::Int;
                            value.i = static_cast<unsigned char>(storage.bytes[offset]);
                            return value;
                        }
                        if (offset < storage.fields.size()) {
                            if (storage.fields[offset].kind == Value::Kind::Invalid)
                                return invalidElement();
                            return storage.fields[offset];
                        }
                    }
                } else if (object.kind == Value::Kind::Ptr) {
                    char byte = 0;
                    if (readByte(object, itemIndex, byte)) {
                        Value value;
                        value.kind = Value::Kind::Int;
                        value.i    = static_cast<unsigned char>(byte);
                        return value;
                    }
                }
                result.status   = HirInterpStatus::UnsupportedExpr;
                result.exitCode = 1;
                result.message =
                    "HIR interpreter cannot read indexed value (kind " +
                    std::to_string(static_cast<unsigned>(object.kind)) + ", object " +
                    std::to_string(object.object) + ", offset " +
                    std::to_string(object.offset) + ", length " +
                    std::to_string(object.length) + ", text bytes " +
                    std::to_string(object.text.size()) + ", cell " +
                    std::to_string(object.cell != nullptr ? 1 : 0) + ", index " +
                    std::to_string(index.i) + ", object type " +
                    std::to_string(node.obj_type) + ", result type " +
                    std::to_string(node.type) + ", cell kind " +
                    std::to_string(object.cell != nullptr
                                       ? static_cast<unsigned>(object.cell->kind)
                                       : static_cast<unsigned>(Value::Kind::Invalid)) +
                    ", cell object " +
                    std::to_string(object.cell != nullptr ? object.cell->object
                                                          : kInvalidObject) +
                    ", cell text bytes " +
                    std::to_string(object.cell != nullptr ? object.cell->text.size() : 0U) +
                    ", array " +
                    std::to_string(node.is_array ? 1 : 0) + ")";
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirAssign>) {
                const auto address = addressOf(node.target, frame, result);
                const auto value   = eval(node.value, frame, result);
                if (result.status != HirInterpStatus::Ok ||
                    !writePointer(address, value, result))
                    return {};
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirEnumValue>) {
                Value value;
                value.kind = Value::Kind::Int;
                value.i    = node.value;
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirMakeNone>) {
                Value value;
                value.kind = Value::Kind::Int;
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirMakeSome>) {
                return eval(node.value, frame, result);
            } else if constexpr (std::is_same_v<T, hir::HirCast> ||
                                 std::is_same_v<T, hir::HirUnionCast>) {
                return eval(node.value, frame, result);
            } else if constexpr (std::is_same_v<T, hir::HirMakeSlice>) {
                const auto source = eval(node.object, frame, result);
                if (result.status != HirInterpStatus::Ok)
                    return {};
                size_t low  = 0;
                size_t high = 0;
                if (node.lo != hir::kInvalidHirExpr) {
                    const auto bound = eval(node.lo, frame, result);
                    if (bound.kind != Value::Kind::Int || bound.i < 0)
                        return {};
                    low = static_cast<size_t>(bound.i);
                }
                if (node.hi != hir::kInvalidHirExpr) {
                    const auto bound = eval(node.hi, frame, result);
                    if (bound.kind != Value::Kind::Int || bound.i < 0)
                        return {};
                    high = static_cast<size_t>(bound.i);
                } else if (source.kind == Value::Kind::String) {
                    high = source.text.size();
                } else if (source.kind == Value::Kind::Slice) {
                    high = source.length;
                } else if (source.kind == Value::Kind::Aggregate &&
                           source.object < objects_.size()) {
                    high = objects_[source.object].fields.size();
                }
                if (high < low)
                    return {};

                Value slice;
                slice.kind   = Value::Kind::Slice;
                slice.length = high - low;
                if (source.kind == Value::Kind::String) {
                    slice.text   = source.text;
                    slice.offset = low;
                } else if (source.kind == Value::Kind::Slice) {
                    slice.object = source.object;
                    slice.text   = source.text;
                    slice.offset = source.offset + low;
                } else if (source.kind == Value::Kind::Aggregate) {
                    slice.object = source.object;
                    slice.offset = low;
                } else if (source.kind == Value::Kind::Ptr) {
                    slice.object = source.object;
                    slice.text   = source.text;
                    slice.cell   = source.cell;
                    slice.offset = source.offset + low;
                } else {
                    return {};
                }
                return slice;
            } else if constexpr (std::is_same_v<T, hir::HirLayoutIntrinsic>) {
                Value value;
                value.kind = Value::Kind::Int;
                if (node.which == hir::HirLayoutIntrinsic::Which::LengthOf) {
                    if (node.string_length != 0U) {
                        value.i = static_cast<int64_t>(node.string_length);
                        return value;
                    }
                    const auto operand = eval(node.operand, frame, result);
                    if (operand.kind == Value::Kind::Slice)
                        value.i = static_cast<int64_t>(operand.length);
                    else if (operand.kind == Value::Kind::String)
                        value.i = static_cast<int64_t>(operand.text.size());
                    else if (operand.kind == Value::Kind::Aggregate &&
                             operand.object < objects_.size())
                        value.i =
                            static_cast<int64_t>(objects_[operand.object].fields.size());
                    else
                        return {};
                    return value;
                }
                if (node.which == hir::HirLayoutIntrinsic::Which::PtrOf) {
                    const auto operand = eval(node.operand, frame, result);
                    if (operand.kind == Value::Kind::Slice) {
                        value.kind   = Value::Kind::Ptr;
                        value.object = operand.object;
                        value.text   = operand.text;
                        value.cell   = operand.cell;
                        value.offset = operand.offset;
                        return value;
                    }
                    if (operand.kind == Value::Kind::String) {
                        value.kind = Value::Kind::Ptr;
                        value.text = operand.text;
                        return value;
                    }
                    return addressOf(node.operand, frame, result);
                }
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirMakeDyn>) {
                Value data;
                if (types_.kindOf(node.source_type) == types::TypeKind::Ptr) {
                    data = eval(node.value, frame, result);
                    if (data.kind == Value::Kind::String)
                        data.kind = Value::Kind::Ptr;
                } else if (node.value_is_place) {
                    data = addressOf(node.value, frame, result);
                } else {
                    auto source = eval(node.value, frame, result);
                    if (source.kind == Value::Kind::Aggregate) {
                        source = cloneAggregate(source);
                        data   = pointerToObject(source.object);
                    } else {
                        const auto object = addObject(node.source_type);
                        if (object == kInvalidObject) {
                            result.status   = HirInterpStatus::InternalError;
                            result.exitCode = 1;
                            return {};
                        }
                        objects_[object].fields.push(source);
                        data = pointerToObject(object);
                    }
                }
                if (result.status != HirInterpStatus::Ok || data.kind != Value::Kind::Ptr)
                    return {};
                Value value;
                value.kind   = Value::Kind::Dyn;
                value.object = data.object;
                value.offset = data.offset;
                value.text   = data.text;
                value.cell   = data.cell;
                value.vtable = node.vtable_name;
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirDynCall>) {
                const auto receiver = eval(node.receiver, frame, result);
                if (result.status != HirInterpStatus::Ok)
                    return {};
                if (receiver.kind != Value::Kind::Dyn) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message  = "HIR interpreter dyn call has a non-dyn receiver";
                    return {};
                }
                const hir::HirVTable *vtable = nullptr;
                for (size_t index = 0; index < module_.getVTableCount(); ++index) {
                    const auto &candidate = module_.getVTable(index);
                    if (candidate.name == receiver.vtable) {
                        vtable = &candidate;
                        break;
                    }
                }
                if (vtable == nullptr) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message =
                        "HIR interpreter cannot find runtime vtable '" +
                        std::string(interner_.lookup(receiver.vtable)) + "' for dyn call";
                    return {};
                }
                if (node.slot_index >= vtable->slots.size()) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message = "HIR interpreter dyn-call slot is outside runtime vtable";
                    return {};
                }
                const auto functionId = vtable->slots[node.slot_index];
                if (functionId >= module_.getFnCount()) {
                    result.status   = HirInterpStatus::UnsupportedExpr;
                    result.exitCode = 1;
                    result.message = "HIR interpreter runtime vtable contains an invalid function";
                    return {};
                }
                const auto &callee = module_.getFn(functionId);
                memory::DynArray<Value> args(frameArena_);
                if (node.has_receiver) {
                    if (callee.params.empty()) {
                        result.status   = HirInterpStatus::UnsupportedExpr;
                        result.exitCode = 1;
                        result.message = "HIR interpreter dyn-call target has no receiver parameter";
                        return {};
                    }
                    Value data;
                    data.kind   = Value::Kind::Ptr;
                    data.object = receiver.object;
                    data.offset = receiver.offset;
                    data.text   = receiver.text;
                    data.cell   = receiver.cell;
                    const auto receiverKind = types_.kindOf(callee.params[0]);
                    if (receiverKind == types::TypeKind::Int ||
                        receiverKind == types::TypeKind::Bool ||
                        receiverKind == types::TypeKind::Float ||
                        receiverKind == types::TypeKind::Slice ||
                        receiverKind == types::TypeKind::Dyn ||
                        receiverKind == types::TypeKind::Optional ||
                        isAggregateType(callee.params[0])) {
                        auto value = readPointer(data, result);
                        if (receiverKind == types::TypeKind::Dyn) {
                            value.kind   = Value::Kind::Dyn;
                            value.object = data.object;
                            value.offset = data.offset;
                            value.text   = data.text;
                            value.cell   = data.cell;
                            value.vtable = receiver.vtable;
                        }
                        args.push(value);
                    } else {
                        args.push(data);
                    }
                }
                for (const auto arg : node.args) {
                    args.push(eval(arg, frame, result));
                    if (result.status != HirInterpStatus::Ok)
                        return {};
                }
                Value value;
                if (!runFunction(callee, args, value, result)) {
                    if (result.status == HirInterpStatus::Ok)
                        result.status = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirSlotAlloca>) {
                if (node.slot >= frame.slots.size()) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirSlotLoad>) {
                if (node.slot >= frame.slots.size()) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                return frame.slots[node.slot];
            } else if constexpr (std::is_same_v<T, hir::HirSlotStore>) {
                const auto value = eval(node.value, frame, result);
                if (node.slot >= frame.slots.size()) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                frame.slots[node.slot] = value.kind == Value::Kind::Aggregate
                                             ? cloneAggregate(value)
                                             : value;
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirSlotAddr>) {
                if (node.slot >= frame.slots.size()) {
                    result.status   = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                return pointerToCell(frame.slots[node.slot]);
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
                if (node.resolved_fn != hir::kInvalidHirFunction) {
                    for (size_t index = 0; index < module_.getFnCount(); ++index) {
                        if (static_cast<hir::HirFunctionId>(index) == node.resolved_fn) {
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
                    result.message = "HIR call target " + std::to_string(node.resolved_fn) +
                                     " is missing";
                    return {};
                }

                memory::DynArray<Value> args(frameArena_);
                for (auto arg_id : node.args) {
                    args.push(eval(arg_id, frame, result));
                    if (result.status != HirInterpStatus::Ok)
                        return {};
                }

                Value value;
                if (!callee->hasBody) {
                    if (!runExtern(*callee, args, value)) {
                        result.status   = HirInterpStatus::MissingExtern;
                        result.exitCode = 1;
                        result.message =
                            "HIR interpreter does not support extern '" +
                            std::string(interner_.lookup(callee->name)) + "'";
                        return {};
                    }
                } else if (!runFunction(*callee, args, value, result)) {
                    if (result.status == HirInterpStatus::Ok)
                        result.status = HirInterpStatus::InternalError;
                    result.exitCode = 1;
                    return {};
                }
                return value;
            } else if constexpr (std::is_same_v<T, hir::HirRet>) {
                return node.value == hir::kInvalidHirExpr ? Value{}
                                                          : eval(node.value, frame, result);
            } else if constexpr (std::is_same_v<T, hir::HirBranch>) {
                const auto cond = eval(node.cond, frame, result);
                if (result.status != HirInterpStatus::Ok)
                    return {};
                const bool truth =
                    cond.kind == Value::Kind::Int
                        ? cond.i != 0
                        : cond.kind == Value::Kind::Ptr
                              ? cond.object != kInvalidObject || cond.cell != nullptr ||
                                    cond.text.data() != nullptr
                              : cond.kind != Value::Kind::Invalid;
                frame.block = truth ? node.then_block : node.else_block;
                return {};
            } else if constexpr (std::is_same_v<T, hir::HirJump>) {
                frame.block = node.target;
                return {};
            } else {
                result.status   = HirInterpStatus::UnsupportedExpr;
                result.exitCode = 1;
                result.message =
                    "HIR interpreter does not support expression kind " +
                    std::to_string(static_cast<unsigned>(hir::exprKind(expr)));
                return {};
            }
        },
        expr);
    if (result.status != HirInterpStatus::Ok && result.message.empty()) {
        result.message = "HIR interpreter failed evaluating expression " + std::to_string(id) +
                         " (kind " +
                         std::to_string(static_cast<unsigned>(hir::exprKind(expr))) + ")";
    }
    return evaluated;
}

bool HirInterpreter::runFunction(const hir::HirFunction &fn, memory::DynArray<Value> &args,
                                 Value &returnValue, HirInterpResult &result) {
    Frame frame(&fn, frameArena_, slotCount_);
    frame.regs.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        auto argument = args[i];
        if (i < fn.params.size() && isAggregateType(fn.params[i]))
            argument = cloneAggregate(argument);
        frame.regs.push(argument);
    }

    size_t block = 0;
    bool stopped = false;
    while (block < fn.blocks.size()) {
        const auto &hir_block = fn.blocks[block];
        for (auto inst : hir_block.insts) {
            eval(inst, frame, result);
            if (result.status != HirInterpStatus::Ok) {
                if (result.message.empty())
                    result.message = "HIR interpreter failed in function '" +
                                     std::string(interner_.lookup(fn.name)) + "', block " +
                                     std::to_string(block) + ", expression " +
                                     std::to_string(inst);
                return false;
            }
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
        if (result.status != HirInterpStatus::Ok) {
            if (result.message.empty())
                result.message = "HIR interpreter failed in function '" +
                                 std::string(interner_.lookup(fn.name)) + "', block " +
                                 std::to_string(block) + ", terminator " +
                                 std::to_string(hir_block.terminator);
            return false;
        }

        if (ret_node != nullptr) {
            returnValue = value;
            stopped     = true;
            break;
        }
        if (const auto *jump = std::get_if<hir::HirJump>(&terminator)) {
            block = jump->target;
            continue;
        }
        if (std::get_if<hir::HirBranch>(&terminator) != nullptr) {
            // A branch terminator must be evaluated: `eval` writes the chosen
            // successor into `frame.block`.  Skipping it leaves `frame.block`
            // at its initial 0 and loops on the entry block forever.
            (void)eval(hir_block.terminator, frame, result);
            if (result.status != HirInterpStatus::Ok)
                return false;
            block = frame.block;
            continue;
        }

        result.status   = HirInterpStatus::UnsupportedExpr;
        result.exitCode = 1;
        result.message = "HIR interpreter encountered an unsupported terminator in function '" +
                         std::string(interner_.lookup(fn.name)) + "', block " +
                         std::to_string(block);
        return false;
    }

    if (!stopped) {
        result.status   = HirInterpStatus::InternalError;
        result.exitCode = 1;
        result.message = "HIR interpreter function '" +
                         std::string(interner_.lookup(fn.name)) +
                         "' reached the end without returning";
        return false;
    }
    return true;
}

bool HirInterpreter::runExtern(const hir::HirFunction &fn, memory::DynArray<Value> &args,
                               Value &returnValue) {
    const auto name = sourceName(interner_.lookup(fn.name));
    const auto makeInt = [](int64_t number) {
        Value value;
        value.kind = Value::Kind::Int;
        value.i    = number;
        return value;
    };
    const auto textOf = [](const Value &value) -> std::string_view {
        if (value.kind == Value::Kind::String)
            return value.text;
        if (value.kind == Value::Kind::Slice && value.object == kInvalidObject &&
            value.offset <= value.text.size() &&
            value.length <= value.text.size() - value.offset)
            return value.text.substr(value.offset, value.length);
        if (value.kind == Value::Kind::Ptr && value.text.data() != nullptr &&
            value.offset <= value.text.size())
            return value.text.substr(value.offset);
        return {};
    };
    const auto cStringLength = [this](const Value &pointer) {
        const auto text = pointer.kind == Value::Kind::String ? pointer.text : pointer.text;
        if ((pointer.kind == Value::Kind::String ||
             (pointer.kind == Value::Kind::Ptr && pointer.text.data() != nullptr)) &&
            pointer.offset <= text.size()) {
            const auto remaining = text.substr(pointer.offset);
            const auto nul       = remaining.find('\0');
            return nul == std::string_view::npos ? remaining.size() : nul;
        }
        size_t length = 0;
        char byte     = 0;
        while (readByte(pointer, length, byte) && byte != '\0')
            ++length;
        return length;
    };
    const auto writeText = [this](const Value &destination, std::string_view text) {
        for (size_t index = 0; index < text.size(); ++index) {
            if (!writeByte(destination, index, text[index]))
                return false;
        }
        return true;
    };

    if (name == "puts" && !args.empty()) {
        const auto text = textOf(args[0]);
        if (!text.empty()) {
            output_.append(text);
        } else {
            const auto length = cStringLength(args[0]);
            for (size_t index = 0; index < length; ++index) {
                char byte = 0;
                if (readByte(args[0], index, byte))
                    output_.append(std::string_view(&byte, 1U));
            }
        }
        output_.append("\n");
        returnValue = makeInt(static_cast<int64_t>(text.empty() ? cStringLength(args[0])
                                                               : text.size()));
        return true;
    }
    if (name == "putchar" && !args.empty()) {
        const char byte = static_cast<char>(args[0].i);
        output_.append(std::string_view(&byte, 1U));
        returnValue = makeInt(args[0].i);
        return true;
    }
    if (name == "aligned_alloc" && args.size() >= 2U && args[1].i >= 0) {
        const auto object = addObject(types::kInvalidType);
        if (object == kInvalidObject)
            return false;
        objects_[object].bytes.resize(static_cast<size_t>(args[1].i), '\0');
        returnValue.kind   = Value::Kind::Ptr;
        returnValue.object = object;
        return true;
    }
    if (name == "free") {
        if (!args.empty() && args[0].object < objects_.size())
            objects_[args[0].object].alive = false;
        returnValue = makeInt(0);
        return true;
    }
    if (name == "memcpy" && args.size() >= 3U && args[2].i >= 0) {
        const auto count = static_cast<size_t>(args[2].i);
        std::vector<char> copy(count);
        for (size_t index = 0; index < count; ++index) {
            if (!readByte(args[1], index, copy[index]))
                return false;
        }
        if (!writeText(args[0], std::string_view(copy.data(), copy.size())))
            return false;
        returnValue = args[0];
        return true;
    }
    if (name == "strlen" && !args.empty()) {
        returnValue = makeInt(static_cast<int64_t>(cStringLength(args[0])));
        return true;
    }
    if (name == "snprintf" && args.size() >= 3U && args[1].i >= 0) {
        const auto format = textOf(args[2]);
        if (args.size() < 4U)
            return false;
        const auto marker = format.find('%');
        if (marker == std::string_view::npos || marker + 1U >= format.size())
            return false;
        char buffer[64]{};
        std::to_chars_result converted{};
        if (format[marker + 1U] == 'u') {
            converted = std::to_chars(std::begin(buffer), std::end(buffer),
                                      static_cast<uint32_t>(args[3].i));
        } else if (format[marker + 1U] == 'd') {
            converted = std::to_chars(std::begin(buffer), std::end(buffer),
                                      static_cast<int32_t>(args[3].i));
        } else {
            return false;
        }
        if (converted.ec != std::errc{})
            return false;
        std::string rendered(format.substr(0, marker));
        rendered.append(buffer, converted.ptr);
        rendered.append(format.substr(marker + 2U));
        const auto capacity = static_cast<size_t>(args[1].i);
        if (capacity > 0U) {
            const auto written = std::min(rendered.size(), capacity - 1U);
            if (!writeText(args[0], std::string_view(rendered.data(), written)) ||
                !writeByte(args[0], written, '\0'))
                return false;
        }
        returnValue = makeInt(static_cast<int64_t>(rendered.size()));
        return true;
    }
    return false;
}

} // namespace zith::interp
