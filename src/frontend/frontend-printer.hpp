#pragma once

#include "frontend/frontend.hpp"

#include <string>

namespace zith::frontend {

void printTokens(const FrontendSnapshot &snapshot);
void printDeclarations(const FrontendSnapshot &snapshot);
[[nodiscard]] std::string dumpCst(const FrontendSnapshot &snapshot);

} // namespace zith::frontend
