#include "target-info.hpp"

namespace zith::backend::interface_ {

    TargetInfo TargetInfo::host() {
        return TargetInfo{Arch::X86_64, OS::Linux, Endian::Little, 8};
    }

} // namespace zith::backend::interface_
