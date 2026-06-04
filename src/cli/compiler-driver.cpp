#include "compiler-driver.hpp"

namespace zith::cli {

    int CompilerDriver::compileFile(const std::string &path) {
        Options opts;
        opts.input_files.push_back(path);
        return compileWithOptions(opts);
    }

    int CompilerDriver::compileWithOptions(const Options &opts) {
        CompilationSession session(opts);
        return session.run();
    }

} // namespace zith::cli
