#include "cli/compilation-session.hpp"
#include "cli/options.hpp"
#include "import/symbol-table.hpp"
#include <cstdio>
#include <string>

int main() {
    // Exactly like the LSP's provideHover
    std::string content = "/// A point in 2D space\nstruct Point {\n    x: i32,\n    y: i32,\n}\n";
    std::string file_path = "/tmp/test_docs.zith";

    zith::cli::Options opts;
    // Don't set stdlib to match LSP test without stdlib path

    zith::cli::CompilationSession session(opts, file_path);
    session.setContent(content);
    session.setBuffered(true);

    bool hasTypeInfo = session.runTo(zith::cli::Stage::TypeChecked);
    fprintf(stderr, "hasTypeInfo=%d\n", hasTypeInfo);

    // Now lookup a symbol and check doc_span
    auto sym_id = session.symbolTable().lookup("Point");
    if (sym_id == zith::import::kInvalidSym) {
        fprintf(stderr, "ERROR: Point not found in symbol table\n");
        return 1;
    }

    auto &data = session.symbolTable().get(sym_id);
    fprintf(stderr, "sym '%.*s' doc_span={%u,%u,%u} file_base=%p\n", (int)data.name.size(),
            data.name.data(), data.doc_span.file, data.doc_span.start, data.doc_span.end,
            (void *)session.tokens().file_base);

    if (data.doc_span.len() > 0 && session.tokens().file_base) {
        std::string_view raw(session.tokens().file_base + data.doc_span.start, data.doc_span.len());
        fprintf(stderr, "doc text: '%.*s'\n", (int)raw.size(), raw.data());
    } else {
        fprintf(stderr, "doc_span is empty or file_base is null!\n");
    }

    return 0;
}
