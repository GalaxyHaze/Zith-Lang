#pragma once

#include <string>
#include <vector>

namespace zith::cli {

// Recursively find all .zith files under the given directory, sorted.
std::vector<std::string> findZithFiles(const std::string &dir);

} // namespace zith::cli
