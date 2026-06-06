#include "cli/commands.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "lexer/lexer.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace zith::cli::commands {

// TODO: implement — parse source, extract doc-test blocks, run them
int cmd_test(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[!] Command 'test' is not implemented yet.\n");
    return 1;
}

int cmd_fmt(const Options &opts) {
    const std::string &src = opts.subcommand_arg.empty() && !opts.input_files.empty()
                                 ? opts.input_files[0]
                                 : opts.subcommand_arg;
    if (src.empty()) {
        std::fprintf(stderr, "error: no input file specified\n");
        return 1;
    }

    if (opts.verbose)
        std::printf("[*] Formatting '%s'...\n", src.c_str());

    diagnostics::DiagnosticEngine diags;
    auto result = lexer::tokenize(src, src, diags);
    if (!result) {
        std::fprintf(stderr, "error: failed to tokenize '%s'\n", src.c_str());
        return 1;
    }

    if (opts.verbose)
        std::printf("[*] Token count: %u\n", result.value().len);

    // TODO: implement actual source formatting
    //       - re-indent based on brace depth
    //       - normalise spacing around operators/punctuation
    //       - preserve comments and docstrings

    std::printf("[ok] '%s' is valid\n", src.c_str());
    return 0;
}

// TODO: implement — parse source comments, generate Markdown/HTML docs
int cmd_docs(const Options &opts) {
    (void)opts;
    std::fprintf(stderr, "[!] Command 'docs' is not implemented yet.\n");
    return 1;
}

} // namespace zith::cli::commands
