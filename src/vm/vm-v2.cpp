#include "vm/vm-v2.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zith::vm {
namespace {

constexpr std::size_t allocationFailure = std::numeric_limits<std::size_t>::max();

struct Frame {
    std::size_t mark = 0;
};

struct RunState {
    const Module *module = nullptr;
    LinearMemory *memory = nullptr;
    std::vector<std::size_t> stringAddresses;
    std::string output;
    std::string message;
    RunStatus status = RunStatus::Ok;
};

auto statusText(RunStatus status) -> std::string_view {
    switch (status) {
    case RunStatus::Ok:
        return "ok";
    case RunStatus::MissingMain:
        return "missing main";
    case RunStatus::Trap:
        return "trap";
    case RunStatus::Unsupported:
        return "unsupported";
    case RunStatus::Oom:
        return "out of linear memory";
    }
    return "unknown status";
}

auto getReg(const std::vector<int64_t> &regs, uint16_t index) -> int64_t {
    if (index < regs.size())
        return regs[index];
    return 0;
}

auto setReg(std::vector<int64_t> &regs, uint16_t index, int64_t value) -> bool {
    if (index >= regs.size())
        return false;
    regs[index] = value;
    return true;
}

auto findFunction(const Module &module, std::string_view name) -> const Function * {
    for (const auto &fn : module.functions)
        if (fn.name == name)
            return &fn;
    return nullptr;
}

auto missingRegister(const Function &fn, uint16_t index, bool pair) -> bool {
    const uint32_t count = static_cast<uint32_t>(fn.regCount);
    return index >= count || (pair && static_cast<uint32_t>(index) + 1 >= count);
}

auto invalidJumpTarget(const Function &fn, const Instr &instr) -> bool {
    return instr.imm >= fn.body.size();
}

/// Return false when the module does not satisfy v2's invariant that every
/// operand names a register/label that exists. This is a static shape check,
/// so invalid guest IR reports Trap before any memory or I/O side effects.
auto validateFunctionShape(const Function &fn) -> bool {
    for (std::size_t pc = 0; pc < fn.body.size(); ++pc) {
        const auto &instr  = fn.body[pc];
        const uint16_t dst = instr.a;
        const uint16_t lhs = instr.b;
        const uint16_t rhs = instr.c;

        if (instr.op > Op::Trap)
            return false;
        switch (instr.op) {
        case Op::LoadConstI32:
        case Op::LoadConstI64:
        case Op::LoadConstF32:
        case Op::LoadConstF64:
        case Op::LoadString:
        case Op::LoadFnRef:
        case Op::LoadExternRef:
        case Op::AllocBytes:
        case Op::MallocBytes:
            if (missingRegister(fn, dst, false))
                return false;
            if (instr.op == Op::AllocBytes || instr.op == Op::MallocBytes) {
                if (missingRegister(fn, lhs, false) || missingRegister(fn, rhs, false))
                    return false;
            }
            break;
        case Op::StoreBytes:
            if (missingRegister(fn, dst, false))
                return false;
            break;
        case Op::LoadBytes:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, false) ||
                missingRegister(fn, rhs, false))
                return false;
            break;
        case Op::FieldPtr:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, false))
                return false;
            break;
        case Op::MakeSlice:
            if (missingRegister(fn, dst, true) || missingRegister(fn, lhs, false) ||
                missingRegister(fn, rhs, false))
                return false;
            break;
        case Op::SlicePtr:
        case Op::SliceLen:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, true))
                return false;
            break;
        case Op::MemCopy:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, false) ||
                missingRegister(fn, rhs, false))
                return false;
            break;
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Rem:
        case Op::Neg:
        case Op::Not:
        case Op::Eq:
        case Op::Ne:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, false) ||
                missingRegister(fn, rhs, false))
                return false;
            break;
        case Op::CallExtern:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, false) ||
                missingRegister(fn, rhs, false) || missingRegister(fn, instr.d, false) ||
                missingRegister(fn, instr.e, false))
                return false;
            break;
        case Op::CallFn:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, false) ||
                missingRegister(fn, rhs, false))
                return false;
            break;
        case Op::CallExternRef:
        case Op::CallFnRef:
            if (missingRegister(fn, dst, false) || missingRegister(fn, lhs, false) ||
                missingRegister(fn, rhs, false) || missingRegister(fn, instr.imm, false))
                return false;
            break;
        case Op::Ret:
            if (missingRegister(fn, dst, false))
                return false;
            break;
        case Op::Branch:
            if (missingRegister(fn, lhs, false) || invalidJumpTarget(fn, instr))
                return false;
            break;
        case Op::Jump:
            if (invalidJumpTarget(fn, instr))
                return false;
            break;
        case Op::Trap:
            break;
        }
    }
    return true;
}

auto runFunction(RunState &state, const Function &fn, std::vector<int64_t> &regs)
    -> std::pair<bool, int64_t> {
    Frame frame;
    (void)frame;

    if (regs.size() < fn.regCount)
        regs.resize(fn.regCount, 0);

    std::size_t pc = 0;
    while (pc < fn.body.size()) {
        const auto &instr  = fn.body[pc];
        const uint16_t dst = instr.a;
        const uint16_t lhs = instr.b;
        const uint16_t rhs = instr.c;

        switch (instr.op) {
        case Op::LoadConstI32:
        case Op::LoadConstI64:
            if (!setReg(regs, dst, instr.imm))
                return {false, 0};
            pc++;
            break;
        case Op::LoadConstF32:
        case Op::LoadConstF64: {
            if (instr.imm >= state.module->f64Constants.size())
                return {false, 0};
            int64_t bits = 0;
            std::memcpy(&bits, &state.module->f64Constants[instr.imm], sizeof(bits));
            if (!setReg(regs, dst, bits))
                return {false, 0};
            pc++;
            break;
        }
        case Op::LoadString:
            if (instr.imm >= state.stringAddresses.size())
                return {false, 0};
            if (!setReg(regs, dst, static_cast<int64_t>(state.stringAddresses[instr.imm])))
                return {false, 0};
            pc++;
            break;
        case Op::LoadFnRef:
            if (instr.imm >= state.module->functions.size())
                return {false, 0};
            if (!setReg(regs, dst, instr.imm))
                return {false, 0};
            pc++;
            break;
        case Op::LoadExternRef:
            if (instr.imm >= state.module->externs.size())
                return {false, 0};
            if (!setReg(regs, dst, instr.imm))
                return {false, 0};
            pc++;
            break;
        case Op::AllocBytes:
        case Op::MallocBytes: {
            const std::size_t count  = static_cast<std::size_t>(getReg(regs, lhs));
            const std::size_t align  = static_cast<std::size_t>(getReg(regs, rhs));
            const std::size_t offset = instr.op == Op::AllocBytes
                                           ? state.memory->allocBytes(count, align)
                                           : state.memory->mallocBytes(count, align);
            if (offset == allocationFailure)
                return {false, 0};
            if (!setReg(regs, dst, static_cast<int64_t>(offset)))
                return {false, 0};
            pc++;
            break;
        }
        case Op::StoreBytes: {
            if (instr.imm >= state.module->strings.size())
                return {false, 0};
            const std::string_view text = state.module->strings[instr.imm];
            const std::size_t offset    = static_cast<std::size_t>(getReg(regs, dst));
            std::vector<uint8_t> bytes(text.begin(), text.end());
            if (!state.memory->write(offset, bytes))
                return {false, 0};
            pc++;
            break;
        }
        case Op::LoadBytes: {
            const std::size_t offset = static_cast<std::size_t>(getReg(regs, lhs));
            const std::size_t count  = static_cast<std::size_t>(getReg(regs, rhs));
            auto bytes               = state.memory->read(offset, count);
            if (bytes.size() != count)
                return {false, 0};
            int64_t value = 0;
            for (std::size_t i = 0; i < count && i < sizeof(value); ++i)
                value = (value << 8) | bytes[i];
            if (!setReg(regs, dst, value))
                return {false, 0};
            pc++;
            break;
        }
        case Op::FieldPtr: {
            const std::size_t base = static_cast<std::size_t>(getReg(regs, lhs));
            if (!setReg(regs, dst, static_cast<int64_t>(base + instr.imm)))
                return {false, 0};
            pc++;
            break;
        }
        case Op::MakeSlice:
            if (!setReg(regs, dst, getReg(regs, lhs)))
                return {false, 0};
            if (!setReg(regs, static_cast<uint16_t>(dst + 1), getReg(regs, rhs)))
                return {false, 0};
            pc++;
            break;
        case Op::SlicePtr:
            if (lhs + 1 >= regs.size())
                return {false, 0};
            if (!setReg(regs, dst, regs[lhs]))
                return {false, 0};
            pc++;
            break;
        case Op::SliceLen:
            if (lhs + 1 >= regs.size())
                return {false, 0};
            if (!setReg(regs, dst, regs[lhs + 1]))
                return {false, 0};
            pc++;
            break;
        case Op::MemCopy: {
            const std::size_t dstOff = static_cast<std::size_t>(getReg(regs, dst));
            const std::size_t srcOff = static_cast<std::size_t>(getReg(regs, lhs));
            const std::size_t count  = static_cast<std::size_t>(getReg(regs, rhs));
            if (!state.memory->copy(dstOff, srcOff, count))
                return {false, 0};
            pc++;
            break;
        }
        case Op::Add:
        case Op::Sub:
        case Op::Mul:
        case Op::Div:
        case Op::Rem: {
            const int64_t left  = getReg(regs, lhs);
            const int64_t right = getReg(regs, rhs);
            int64_t result      = 0;
            if (instr.op == Op::Div || instr.op == Op::Rem) {
                if (right == 0)
                    return {false, 0};
                result = instr.op == Op::Div ? left / right : left % right;
            } else if (instr.op == Op::Add) {
                result = left + right;
            } else if (instr.op == Op::Sub) {
                result = left - right;
            } else {
                result = left * right;
            }
            if (!setReg(regs, dst, result))
                return {false, 0};
            pc++;
            break;
        }
        case Op::Neg:
            if (!setReg(regs, dst, -getReg(regs, lhs)))
                return {false, 0};
            pc++;
            break;
        case Op::Not:
            if (!setReg(regs, dst, getReg(regs, lhs) == 0 ? 1 : 0))
                return {false, 0};
            pc++;
            break;
        case Op::Eq:
        case Op::Ne:
        case Op::Lt:
        case Op::Le:
        case Op::Gt:
        case Op::Ge: {
            const int64_t left  = getReg(regs, lhs);
            const int64_t right = getReg(regs, rhs);
            bool result         = false;
            switch (instr.op) {
            case Op::Eq:
                result = left == right;
                break;
            case Op::Ne:
                result = left != right;
                break;
            case Op::Lt:
                result = left < right;
                break;
            case Op::Le:
                result = left <= right;
                break;
            case Op::Gt:
                result = left > right;
                break;
            case Op::Ge:
                result = left >= right;
                break;
            default:
                break;
            }
            if (!setReg(regs, dst, result ? 1 : 0))
                return {false, 0};
            pc++;
            break;
        }
        case Op::CallExtern: {
            if (instr.imm >= state.module->externs.size())
                return {false, 0};
            const std::string_view name = state.module->externs[instr.imm];
            const int64_t arg0          = getReg(regs, lhs);
            const int64_t arg1          = getReg(regs, rhs);
            const int64_t arg2          = getReg(regs, instr.d);
            const int64_t arg3          = getReg(regs, instr.e);
            int64_t result              = 0;
            if (name == "puts") {
                const std::string_view text = state.memory->cstring(static_cast<std::size_t>(arg0));
                if (text.empty())
                    return {false, 0};
                state.output.append(text.data(), text.size());
                state.output.push_back('\n');
            } else if (name == "putchar") {
                state.output.push_back(static_cast<char>(arg0));
            } else if (name == "malloc") {
                result = static_cast<int64_t>(
                    state.memory->mallocBytes(static_cast<std::size_t>(arg0), 1));
            } else if (name == "free") {
                if (arg0 != 0)
                    state.memory->freeBytes(static_cast<std::size_t>(arg0));
            } else if (name == "snprintf") {
                const std::string_view text = state.memory->cstring(static_cast<std::size_t>(arg2));
                if (text.empty())
                    return {false, 0};
                std::string formatted;
                if (text.find("%u") != std::string_view::npos)
                    formatted = std::to_string(static_cast<unsigned long>(arg3));
                else if (text.find("%d") != std::string_view::npos)
                    formatted = std::to_string(arg3);
                else if (text.find("%g") != std::string_view::npos) {
                    double value = 0;
                    std::memcpy(&value, &arg3, sizeof(value));
                    formatted = std::to_string(value);
                } else
                    formatted = std::string(text);
                if (arg1 <= 0)
                    return {false, 0};
                if (formatted.size() >= static_cast<std::size_t>(arg1))
                    formatted.resize(static_cast<std::size_t>(arg1) - 1);
                state.memory->writeString(static_cast<std::size_t>(arg0), formatted);
                result = static_cast<int64_t>(formatted.size());
            } else if (name == "strlen") {
                const std::string_view text = state.memory->cstring(static_cast<std::size_t>(arg0));
                result                      = static_cast<int64_t>(text.size());
            } else if (name == "memcpy") {
                if (!state.memory->copy(static_cast<std::size_t>(arg0),
                                        static_cast<std::size_t>(arg1),
                                        static_cast<std::size_t>(arg2)))
                    return {false, 0};
                result = arg0;
            } else {
                return {false, 0};
            }
            if (!setReg(regs, dst, result))
                return {false, 0};
            pc++;
            break;
        }
        case Op::CallExternRef: {
            const int64_t ref = getReg(regs, lhs);
            if (ref < 0 || static_cast<std::size_t>(ref) >= state.module->externs.size())
                return {false, 0};
            const std::string_view name = state.module->externs[static_cast<std::size_t>(ref)];
            const int64_t arg0          = getReg(regs, rhs);
            const int64_t arg1          = getReg(regs, instr.imm);
            const int64_t arg2          = getReg(regs, instr.d);
            const int64_t arg3          = getReg(regs, instr.e);
            int64_t result              = 0;
            if (name == "puts") {
                const std::string_view text = state.memory->cstring(static_cast<std::size_t>(arg0));
                if (text.empty())
                    return {false, 0};
                state.output.append(text.data(), text.size());
                state.output.push_back('\n');
            } else if (name == "putchar") {
                state.output.push_back(static_cast<char>(arg0));
            } else if (name == "malloc") {
                result = static_cast<int64_t>(
                    state.memory->mallocBytes(static_cast<std::size_t>(arg0), 1));
            } else if (name == "free") {
                if (arg0 != 0)
                    state.memory->freeBytes(static_cast<std::size_t>(arg0));
            } else if (name == "snprintf") {
                const std::string_view text = state.memory->cstring(static_cast<std::size_t>(arg2));
                if (text.empty())
                    return {false, 0};
                std::string formatted;
                if (text.find("%u") != std::string_view::npos)
                    formatted = std::to_string(static_cast<unsigned long>(arg3));
                else if (text.find("%d") != std::string_view::npos)
                    formatted = std::to_string(arg3);
                else if (text.find("%g") != std::string_view::npos)
                    formatted = std::to_string(arg3);
                else
                    formatted = std::string(text);
                if (arg1 > 0 && arg3 >= 0)
                    formatted.resize(static_cast<std::size_t>(arg1));
                state.memory->writeString(static_cast<std::size_t>(arg0), formatted);
                result = static_cast<int64_t>(formatted.size());
            } else if (name == "strlen") {
                const std::string_view text = state.memory->cstring(static_cast<std::size_t>(arg0));
                result                      = static_cast<int64_t>(text.size());
            } else if (name == "memcpy") {
                if (!state.memory->copy(static_cast<std::size_t>(arg0),
                                        static_cast<std::size_t>(arg1),
                                        static_cast<std::size_t>(arg2)))
                    return {false, 0};
                result = arg0;
            } else {
                return {false, 0};
            }
            if (!setReg(regs, dst, result))
                return {false, 0};
            pc++;
            break;
        }
        case Op::CallFn: {
            if (instr.imm >= state.module->functions.size())
                return {false, 0};
            const auto *callee = &state.module->functions[instr.imm];
            std::vector<int64_t> calleeRegs(callee->regCount, 0);
            if (callee->paramCount > 0)
                calleeRegs[0] = getReg(regs, lhs);
            if (callee->paramCount > 1)
                calleeRegs[1] = getReg(regs, rhs);
            const auto [ok, ret] = runFunction(state, *callee, calleeRegs);
            if (!ok)
                return {false, 0};
            if (!setReg(regs, dst, ret))
                return {false, 0};
            pc++;
            break;
        }
        case Op::CallFnRef: {
            const int64_t ref = getReg(regs, lhs);
            if (ref < 0 || static_cast<std::size_t>(ref) >= state.module->functions.size())
                return {false, 0};
            const auto *callee = &state.module->functions[static_cast<std::size_t>(ref)];
            std::vector<int64_t> calleeRegs(callee->regCount, 0);
            if (callee->paramCount > 0)
                calleeRegs[0] = getReg(regs, rhs);
            if (callee->paramCount > 1)
                calleeRegs[1] = getReg(regs, instr.imm);
            const auto [ok, ret] = runFunction(state, *callee, calleeRegs);
            if (!ok)
                return {false, 0};
            if (!setReg(regs, dst, ret))
                return {false, 0};
            pc++;
            break;
        }
        case Op::Ret:
            return {true, getReg(regs, dst)};
        case Op::Branch:
            if (getReg(regs, lhs) != 0)
                pc = instr.imm;
            else
                pc++;
            break;
        case Op::Jump:
            pc = instr.imm;
            break;
        case Op::Trap:
            return {false, 0};
        }
    }

    return {false, 0};
}

} // namespace

Vm::Vm(std::size_t linearMemoryCapacity) : memory_(linearMemoryCapacity) {}

auto Vm::runMain(const Module &module) -> RunResult {
    RunResult result;
    const auto *main = findFunction(module, "main");
    if (main == nullptr) {
        result.status  = RunStatus::MissingMain;
        result.message = "the typed IR has no main function";
        return result;
    }
    for (const auto &fn : module.functions) {
        if (!validateFunctionShape(fn)) {
            result.status  = RunStatus::Trap;
            result.message = "invalid typed IR shape";
            return result;
        }
    }

    RunState state;
    state.module = &module;
    state.memory = &memory_;
    state.stringAddresses.reserve(module.strings.size());
    for (std::size_t i = 0; i < module.strings.size(); ++i) {
        const std::string_view text = module.strings[i];
        const std::size_t offset    = memory_.allocBytes(text.size() + 1, 1);
        if (offset == allocationFailure) {
            result.status  = RunStatus::Oom;
            result.message = "cannot materialize string constants";
            return result;
        }
        std::vector<uint8_t> bytes(text.begin(), text.end());
        bytes.push_back(0);
        if (!memory_.write(offset, bytes)) {
            result.status  = RunStatus::Trap;
            result.message = "cannot write string constant";
            return result;
        }
        state.stringAddresses.push_back(offset);
    }
    std::vector<int64_t> regs(main->regCount, 0);
    const auto [ok, ret] = runFunction(state, *main, regs);
    if (!ok) {
        result.status  = RunStatus::Trap;
        result.message = state.message.empty() ? statusText(RunStatus::Trap) : state.message;
        return result;
    }

    result.status   = RunStatus::Ok;
    result.exitCode = ret;
    result.output   = std::move(state.output);
    return result;
}

} // namespace zith::vm
