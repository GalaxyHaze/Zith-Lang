#pragma once

#include "ir/exec-ir.hpp"
#include "memory/arena.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace zith::interp {

enum class IrVmStatus : uint8_t {
    Ok,
    MissingMain,
    Trap,
    Unsupported,
};

struct IrVmResult {
    IrVmStatus status = IrVmStatus::Ok;
    int64_t exitCode  = 0;
    std::string message;
    std::string output;
};

class IrVm {
public:
    explicit IrVm(memory::Arena &arena);
    IrVmResult runMain(ir::Module &module);

private:
    memory::Arena &arena_;
    std::string output_;

    static const ir::Function *findMain(ir::Module &module);
};

} // namespace zith::interp
