#pragma once

#include "vm/typed-ir.hpp"

#include <string>

namespace zith::vm {

[[nodiscard]] std::string dump(const Module &module);

} // namespace zith::vm
