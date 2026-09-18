#pragma once

#include "vm/typed-ir.hpp"
#include "vm/vm-memory.hpp"

#include <cstdint>
#include <string>

namespace zith::vm {

enum class RunStatus : uint8_t {
    Ok,
    MissingMain,
    Trap,
    Unsupported,
    Oom,
};

struct RunResult {
    RunStatus status = RunStatus::Ok;
    int64_t exitCode = 0;
    std::string message;
    std::string output;
};

class Vm {
public:
    explicit Vm(std::size_t linearMemoryCapacity = 1U << 20);

    auto runMain(const Module &module) -> RunResult;

    [[nodiscard]] auto memory() const noexcept -> const LinearMemory & {
        return memory_;
    }

private:
    LinearMemory memory_;
};

} // namespace zith::vm
