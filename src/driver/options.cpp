#include "options.hpp"
#include <cstdio>
#include <cstring>

namespace zith::driver {

    Options parseArgs(int argc, char** argv) {
        Options opts;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--tokens") == 0) {
                opts.emit_tokens = true;
                opts.print_tokens = true;
            } else if (std::strcmp(argv[i], "--emit-ast") == 0) {
                opts.emit_ast = true;
            } else if (std::strcmp(argv[i], "--emit-hir") == 0) {
                opts.emit_hir = true;
            } else if (std::strcmp(argv[i], "--emit-mir") == 0) {
                opts.emit_mir = true;
            } else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                opts.output_file = argv[++i];
            } else {
                opts.input_files.push_back(argv[i]);
            }
        }
        return opts;
    }

}
