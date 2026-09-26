#include "ir-vm.hpp"

#include "ir/exec-ir.hpp"

#include <algorithm>
#include <cstdint>

namespace zith::interp {
namespace {

enum class RunStatus : uint8_t { Continue, Returned, Trapped };

RunStatus runFunction(const ir::Function &fn, ir::Module &module, memory::Arena &arena,
                      std::string &output, int64_t &ret, memory::DynArray<int64_t> &regs) {
    memory::DynArray<int64_t> slots(arena);
    if (regs.size() < fn.registerCount)
        regs.resize(fn.registerCount, 0);
    if (regs.size() < fn.registerCount)
        return RunStatus::Trapped;
    if (slots.size() < fn.slotCount)
        slots.resize(fn.slotCount, 0);

    size_t pc = 0;
    while (pc < fn.body.size()) {
        const auto &instr  = fn.body[pc];
        const uint32_t dst = instr.a;
        const uint32_t lhs = instr.b;
        const uint32_t rhs = instr.c;

        switch (instr.op) {
        case ir::Op::LoadConst:
            if (dst >= regs.size() || instr.imm >= module.constants.size())
                return RunStatus::Trapped;
            regs[dst] = module.constants[instr.imm];
            pc++;
            break;
        case ir::Op::LoadString:
            if (dst >= regs.size() || instr.imm >= module.strings.size())
                return RunStatus::Trapped;
            regs[dst] = instr.imm;
            pc++;
            break;
        case ir::Op::Copy:
            if (dst >= regs.size() || lhs >= regs.size())
                return RunStatus::Trapped;
            regs[dst] = regs[lhs];
            pc++;
            break;
        case ir::Op::Add:
        case ir::Op::Sub:
        case ir::Op::Mul:
        case ir::Op::Div:
        case ir::Op::Rem:
        case ir::Op::Eq:
        case ir::Op::Ne:
        case ir::Op::Lt:
        case ir::Op::Le:
        case ir::Op::Gt:
        case ir::Op::Ge:
        case ir::Op::And:
        case ir::Op::Or:
        case ir::Op::Xor:
        case ir::Op::Shl:
        case ir::Op::Shr:
            if (dst >= regs.size() || lhs >= regs.size() || rhs >= regs.size())
                return RunStatus::Trapped;
            switch (instr.op) {
            case ir::Op::Add:
                regs[dst] = regs[lhs] + regs[rhs];
                break;
            case ir::Op::Sub:
                regs[dst] = regs[lhs] - regs[rhs];
                break;
            case ir::Op::Mul:
                regs[dst] = regs[lhs] * regs[rhs];
                break;
            case ir::Op::Div:
                if (regs[rhs] == 0)
                    return RunStatus::Trapped;
                regs[dst] = regs[lhs] / regs[rhs];
                break;
            case ir::Op::Rem:
                if (regs[rhs] == 0)
                    return RunStatus::Trapped;
                regs[dst] = regs[lhs] % regs[rhs];
                break;
            case ir::Op::Eq:
                regs[dst] = regs[lhs] == regs[rhs];
                break;
            case ir::Op::Ne:
                regs[dst] = regs[lhs] != regs[rhs];
                break;
            case ir::Op::Lt:
                regs[dst] = regs[lhs] < regs[rhs];
                break;
            case ir::Op::Le:
                regs[dst] = regs[lhs] <= regs[rhs];
                break;
            case ir::Op::Gt:
                regs[dst] = regs[lhs] > regs[rhs];
                break;
            case ir::Op::Ge:
                regs[dst] = regs[lhs] >= regs[rhs];
                break;
            case ir::Op::And:
                regs[dst] = static_cast<int64_t>(regs[lhs] != 0 && regs[rhs] != 0);
                break;
            case ir::Op::Or:
                regs[dst] = static_cast<int64_t>(regs[lhs] != 0 || regs[rhs] != 0);
                break;
            case ir::Op::Xor:
                regs[dst] = regs[lhs] ^ regs[rhs];
                break;
            case ir::Op::Shl:
                if (regs[rhs] < 0)
                    return RunStatus::Trapped;
                regs[dst] = regs[lhs] << static_cast<unsigned>(regs[rhs]);
                break;
            case ir::Op::Shr:
                if (regs[rhs] < 0)
                    return RunStatus::Trapped;
                regs[dst] = regs[lhs] >> static_cast<unsigned>(regs[rhs]);
                break;
            case ir::Op::LoadConst:
            case ir::Op::LoadString:
            case ir::Op::Copy:
            case ir::Op::Neg:
            case ir::Op::Not:
            case ir::Op::BitNot:
            case ir::Op::SlotLoad:
            case ir::Op::SlotStore:
            case ir::Op::CallFn:
            case ir::Op::CallExtern:
            case ir::Op::Ret:
            case ir::Op::Branch:
            case ir::Op::Jump:
            case ir::Op::Trap:
                break;
            }
            pc++;
            break;
        case ir::Op::Neg:
        case ir::Op::Not:
        case ir::Op::BitNot:
            if (dst >= regs.size() || lhs >= regs.size())
                return RunStatus::Trapped;
            if (instr.op == ir::Op::Neg)
                regs[dst] = -regs[lhs];
            else if (instr.op == ir::Op::Not)
                regs[dst] = regs[lhs] == 0;
            else
                regs[dst] = ~regs[lhs];
            pc++;
            break;
        case ir::Op::SlotLoad:
            if (dst >= regs.size() || instr.imm >= slots.size())
                return RunStatus::Trapped;
            regs[dst] = slots[instr.imm];
            pc++;
            break;
        case ir::Op::SlotStore:
            if (instr.imm >= slots.size() || lhs >= regs.size())
                return RunStatus::Trapped;
            slots[instr.imm] = regs[lhs];
            pc++;
            break;
        case ir::Op::CallExtern:
            if (instr.imm >= module.externs.size() || instr.b >= regs.size())
                return RunStatus::Trapped;
            if (module.externs[instr.imm] == "puts") {
                const auto stringIndex = static_cast<size_t>(regs[instr.b]);
                if (stringIndex >= module.strings.size())
                    return RunStatus::Trapped;
                const auto text = module.strings[stringIndex];
                output.append(text.data(), text.size());
                output.push_back('\n');
            } else if (module.externs[instr.imm] == "putchar") {
                output.push_back(static_cast<char>(regs[instr.b]));
            } else {
                return RunStatus::Trapped;
            }
            pc++;
            break;
        case ir::Op::Ret:
            if (fn.returnIsExitCode && fn.returnRegister < regs.size()) {
                ret = regs[fn.returnRegister];
            }
            return RunStatus::Returned;
        case ir::Op::Branch:
            if (lhs >= regs.size())
                return RunStatus::Trapped;
            pc = regs[lhs] != 0 ? instr.imm : pc + 1U;
            break;
        case ir::Op::Jump:
            if (instr.imm >= fn.body.size())
                return RunStatus::Trapped;
            pc = instr.imm;
            break;
        case ir::Op::CallFn:
            if (instr.imm >= module.functions.size() || instr.b >= regs.size() ||
                instr.c >= regs.size()) {
                return RunStatus::Trapped;
            }
            {
                const auto &callee = module.functions[instr.imm];
                if (callee.paramCount > 2)
                    return RunStatus::Trapped;
                memory::DynArray<int64_t> calleeRegs(arena);
                calleeRegs.resize(callee.registerCount, 0);
                if (callee.paramCount > 0) {
                    if (instr.b >= regs.size()) {
                        return RunStatus::Trapped;
                    }
                    calleeRegs[0] = regs[instr.b];
                }
                if (callee.paramCount > 1) {
                    if (instr.c >= regs.size()) {
                        return RunStatus::Trapped;
                    }
                    calleeRegs[1] = regs[instr.c];
                }
                int64_t calleeRet = 0;
                const auto callStatus =
                    runFunction(callee, module, arena, output, calleeRet, calleeRegs);
                if (callStatus == RunStatus::Trapped)
                    return RunStatus::Trapped;
                if (instr.a < regs.size())
                    regs[instr.a] = calleeRet;
                else
                    return RunStatus::Trapped;
            }
            pc++;
            break;
        case ir::Op::Trap:
            return RunStatus::Trapped;
        }
    }
    return RunStatus::Trapped;
}

} // namespace

IrVm::IrVm(memory::Arena &arena) : arena_(arena) {}

const ir::Function *IrVm::findMain(ir::Module &module) {
    for (size_t i = 0; i < module.functions.size(); ++i) {
        if (module.functions[i].name == "main")
            return &module.functions[i];
    }
    return nullptr;
}

IrVmResult IrVm::runMain(ir::Module &module) {
    IrVmResult result;
    const auto *main = findMain(module);
    if (main == nullptr) {
        result.status  = IrVmStatus::MissingMain;
        result.message = "the execution IR has no main function";
        return result;
    }

    memory::DynArray<int64_t> regs(arena_);
    int64_t ret       = 0;
    const auto status = runFunction(*main, module, arena_, output_, ret, regs);
    if (status == RunStatus::Trapped) {
        result.status  = IrVmStatus::Trap;
        result.message = "the execution IR trapped";
        return result;
    }

    result.status   = IrVmStatus::Ok;
    result.exitCode = ret;
    result.output   = std::move(output_);
    return result;
}

} // namespace zith::interp
