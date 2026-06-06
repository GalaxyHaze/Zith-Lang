#include "cli/commands.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/emitter.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "parser/source-map.hpp"
#include "memory/arena.hpp"

#include <cstdio>
#include <string>

namespace zith::cli::commands {

static bool runFrontend(const Options &opts, diagnostics::DiagnosticEngine &diags) {
    const std::string &src = opts.subcommand_arg.empty() && !opts.input_files.empty()
                                 ? opts.input_files[0]
                                 : opts.subcommand_arg;
    if (src.empty()) {
        std::fprintf(stderr, "error: no input file specified\n");
        return false;
    }

    auto file = parser::SourceMap::load_file(src);

    if (!file) {
        std::fprintf(stderr, "error: failed to open file '%s'\n", src.c_str());
        return false;
    }

    auto result = lexer::tokenize(file.value(), diags);
    if (!result) {
        std::fprintf(stderr, "error: failed to tokenize '%s'\n", src.c_str());
        return false;
    }

    auto &tokens = result.value();

    if (opts.print_tokens)
        lexer::printTokens(tokens);

    if (opts.emit_tokens)
        return true;

    memory::Arena arena;
    ast::AstBuilder builder(arena);
    parser::Parser parser(tokens, builder, diags);
    auto program = parser.parseProgram();

    if (diags.hasErrors()) {
        diagnostics::Emitter emitter(diags);
        emitter.emit();
        return false;
    }

    if (opts.verbose)
        std::printf("[*] Parsed successfully: %s\n", src.c_str());

    return true;
}

int cmd_check(const Options &opts) {
    diagnostics::DiagnosticEngine diags;
    if (!runFrontend(opts, diags))
        return 1;
    std::printf("[ok] Check passed\n");
    return 0;
}

// TODO: implement actual codegen (object/bytecode emission)
int cmd_compile(const Options &opts) {
    // Chain: compile = check + codegen
    diagnostics::DiagnosticEngine diags;
    if (!runFrontend(opts, diags))
        return 1;

    // TODO: emit .o for native targets, .nbc for interpreted targets
    const std::string out = opts.output_file.empty() ? "a.o" : opts.output_file;
    std::printf("[ok] Compiled to '%s'\n", out.c_str());
    return 0;
}

// TODO: implement actual linker invocation
int cmd_build(const Options &opts) {
    // Chain: build = compile + link
    if (cmd_compile(opts) != 0)
        return 1;

    // TODO: invoke system linker (or embedded linker) to produce final binary
    const std::string out = opts.output_file.empty() ? "a.out" : opts.output_file;
    std::printf("[ok] Build succeeded: '%s'\n", out.c_str());
    return 0;
}

} // namespace zith::cli::commands
