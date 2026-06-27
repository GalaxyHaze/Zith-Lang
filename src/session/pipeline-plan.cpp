#include "pipeline-plan.hpp"

namespace zith::session {

bool PipelinePlan::shouldStop() const noexcept {
    return current >= target;
}

void PipelinePlan::advance() noexcept {
    current = static_cast<Stage>(static_cast<uint8_t>(current) + 1);
}

} // namespace zith::session
