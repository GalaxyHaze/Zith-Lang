#include "cli/files.hpp"

#include <algorithm>
#include <filesystem>

namespace zith::cli {

namespace fs = std::filesystem;

std::vector<std::string> findZithFiles(const std::string &dir) {
    std::vector<std::string> files;
    if (!fs::is_directory(dir))
        return files;
    for (const auto &entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".zith")
            files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace zith::cli
