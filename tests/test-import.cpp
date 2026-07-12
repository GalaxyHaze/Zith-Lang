#include "ast/ast-builder.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "lexer/lexer.hpp"
#include "memory/arena.hpp"
#include "memory/source-map.hpp"
#include "memory/string-interner.hpp"
#include "parser/parser.hpp"
#include "symbols/symbol-table.hpp"
#include "test-common.hpp"

#include <cstdio>
#include <string_view>

using namespace zith;

#ifndef CHECK_GE
#define CHECK_GE(a, b, msg) CHECK((a) >= (b), msg)
#endif

struct ImportTest {
    memory::Arena arena;
    memory::StringInterner interner;
    memory::SourceMap sourceMap;
    diagnostics::DiagnosticEngine diags;

    ImportTest() : interner(arena), diags(arena) {}

    struct Result {
        ast::AstBuilder *builder;
        ast::ProgramNode *program;
        bool ok;
        size_t errorCount;
    };

    Result scan(std::string_view input) {
        auto addResult = sourceMap.addFile("test.zith", input);
        if (!addResult)
            return {nullptr, nullptr, false, 0};
        auto fileId      = addResult.value();
        auto tokenResult = lexer::tokenize(sourceMap, arena, fileId, diags);
        if (!tokenResult) {
            size_t errs = 0;
            for (auto &d : diags.all())
                if (d.severity == diagnostics::Severity::Error)
                    errs++;
            return {nullptr, nullptr, false, errs};
        }
        lexer::TokenStream tokens = std::move(tokenResult.value());
        auto *builder             = arena.make<ast::AstBuilder>(arena, interner);
        auto *prog                = arena.make<ast::ProgramNode>(arena);
        symbols::SymbolTable syms(arena, &interner);
        parser::Parser parser(&tokens, builder, &diags);
        parser::scan(parser, syms);
        *prog       = std::move(parser.program);
        size_t errs = 0;
        for (auto &d : diags.all())
            if (d.severity == diagnostics::Severity::Error)
                errs++;
        return {builder, prog, errs == 0, errs};
    }
};

// ── Helpers ──────────────────────────────────────────────────────────

static int count_decls(const ast::ProgramNode &prog) {
    int n = 0;
    for (auto id : prog.decls)
        if (id != ast::kInvalidDecl)
            n++;
    return n;
}

static const ast::ImportNode *get_import(const ast::ProgramNode &prog, ast::AstBuilder *bld,
                                         int idx) {
    int n = 0;
    for (auto id : prog.decls) {
        if (id == ast::kInvalidDecl)
            continue;
        auto &decl = bld->getDecl(id);
        auto *imp  = std::get_if<ast::ImportNode>(&decl);
        if (!imp)
            continue;
        if (n++ == idx)
            return imp;
    }
    return nullptr;
}

// ── Tests ────────────────────────────────────────────────────────────

static void test_from_simple() {
    ImportTest t;
    auto r = t.scan("from std/io/console");
    CHECK(r.ok, "from std/io/console scan succeeds");
    CHECK_EQ(count_decls(*r.program), 1, "1 decl");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(imp->is_from, "is_from is true");
    CHECK(!imp->is_export, "is_export is false");
    CHECK(!imp->is_asset, "is_asset is false");
    CHECK(imp->alias.empty(), "no alias");
    CHECK_EQ(imp->path.size(), 3u, "3 path segments");
    CHECK(imp->symbols.empty(), "no selective symbols");
}

static void test_import_simple() {
    ImportTest t;
    auto r = t.scan("import std/collections/map");
    CHECK(r.ok, "import std/collections/map scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(!imp->is_from, "is_from is false");
    CHECK(!imp->is_export, "is_export is false");
    CHECK(imp->alias.empty(), "no alias");
    CHECK_EQ(imp->path.size(), 3u, "3 path segments");
}

static void test_export_behaves_like_import() {
    ImportTest t;
    auto r = t.scan("export std/io/console");
    CHECK(r.ok, "export std/io/console scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(!imp->is_from, "export has is_from = false");
    CHECK(imp->is_export, "export has is_export = true");
}

static void test_from_with_alias() {
    ImportTest t;
    auto r = t.scan("from std/io/console as con");
    CHECK(r.ok, "from ... as alias scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(imp->is_from, "is_from is true");
    CHECK_EQ(imp->alias, "con", "alias is 'con'");
}

static void test_import_with_alias() {
    ImportTest t;
    auto r = t.scan("import std/collections/map as map_mod");
    CHECK(r.ok, "import ... as alias scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(!imp->is_from, "is_from is false");
    CHECK_EQ(imp->alias, "map_mod", "alias is 'map_mod'");
}

static void test_from_with_selective() {
    ImportTest t;
    auto r = t.scan("from std/io/console { println, log }");
    CHECK(r.ok, "from ... { sym1, sym2 } scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(!imp->symbols.empty(), "has selective symbols");
    CHECK_EQ(imp->symbols.size(), 2u, "2 selective symbols");
    CHECK_EQ(imp->symbols[0].name, "println", "first symbol is 'println'");
    CHECK(imp->symbols[0].alias.empty(), "first symbol has no alias");
    CHECK_EQ(imp->symbols[1].name, "log", "second symbol is 'log'");
}

static void test_from_with_selective_alias() {
    ImportTest t;
    auto r = t.scan("from std/io/console { println as p, log }");
    CHECK(r.ok, "from ... { sym1 as a, sym2 } scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK_EQ(imp->symbols.size(), 2u, "2 selective symbols");
    CHECK_EQ(imp->symbols[0].name, "println", "first symbol name");
    CHECK_EQ(imp->symbols[0].alias, "p", "first symbol alias");
    CHECK_EQ(imp->symbols[1].name, "log", "second symbol name");
    CHECK(imp->symbols[1].alias.empty(), "second symbol has no alias");
}

static void test_import_with_selective() {
    ImportTest t;
    auto r = t.scan("import std/collections/map { HashMap, TreeMap }");
    CHECK(r.ok, "import ... { sym1, sym2 } scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(!imp->is_from, "is_from is false");
    CHECK_EQ(imp->symbols.size(), 2u, "2 selective symbols");
}

static void test_export_with_selective() {
    ImportTest t;
    auto r = t.scan("export std/io/console { println }");
    CHECK(r.ok, "export ... { sym } scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(imp->is_export, "is_export is true");
    CHECK_EQ(imp->symbols.size(), 1u, "1 selective symbol");
}

static void test_from_with_depth() {
    ImportTest t;
    auto r = t.scan("from std/io(3)");
    CHECK(r.ok, "from ...(depth) scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK_EQ(imp->import_depth, 3, "import depth is 3");
}

static void test_from_with_unlimited_depth() {
    ImportTest t;
    auto r = t.scan("from std/io(..)");
    CHECK(r.ok, "from ...(..) scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK_EQ(imp->import_depth, -1, "import depth is -1 (unlimited)");
}

static void test_from_asset() {
    ImportTest t;
    auto r = t.scan("from assets/data.json as Data");
    CHECK(r.ok, "from assets/... as alias scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(imp->is_asset, "is_asset is true");
    CHECK_EQ(imp->path[0], "assets", "first path segment is 'assets'");
    CHECK_EQ(imp->alias, "Data", "alias is 'Data'");
}

static void test_from_asset_requires_alias() {
    ImportTest t;
    auto r = t.scan("from assets/data.json");
    CHECK(!r.ok, "from assets/... without alias produces error");
    CHECK_GE(r.errorCount, 1u, "at least 1 error for missing alias");
}

static void test_relative_import() {
    ImportTest t;
    auto r = t.scan("from ../lib/utils");
    CHECK(r.ok, "from ../relative/path scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK_EQ(imp->path.size(), 3u, "3 path segments");
    CHECK_EQ(imp->path[0], "..", "first segment is '..'");
}

static void test_from_empty_path() {
    ImportTest t;
    auto r = t.scan("from");
    CHECK(!r.ok, "from with no path produces error");
    CHECK_GE(r.errorCount, 1u, "at least 1 error");
}

static void test_import_empty_path() {
    ImportTest t;
    auto r = t.scan("import");
    CHECK(!r.ok, "import with no path produces error");
    CHECK_GE(r.errorCount, 1u, "at least 1 error");
}

static void test_from_selective_empty() {
    ImportTest t;
    auto r = t.scan("from std/io/console {}");
    CHECK(r.ok, "from ... {} (empty list) scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK(imp->symbols.empty(), "empty symbols list");
}

static void test_selective_trailing_comma() {
    ImportTest t;
    auto r = t.scan("from std/io { println, }");
    CHECK(r.ok, "from ... { sym, } scan succeeds");
    auto *imp = get_import(*r.program, r.builder, 0);
    CHECK(imp != nullptr, "import node exists");
    CHECK_EQ(imp->symbols.size(), 1u, "1 symbol (trailing comma ignored)");
}

static void test_import_relative() {
    ImportTest t;
    auto r = t.scan("import ../helper");
    CHECK(r.ok, "import ../helper scan succeeds");
}

static void test_export_relative() {
    ImportTest t;
    auto r = t.scan("export ../lib");
    CHECK(r.ok, "export ../lib scan succeeds");
}

static void test_multiple_imports() {
    ImportTest t;
    auto r = t.scan("from std/io/console\nimport std/collections/map\nexport std/math");
    CHECK(r.ok, "multiple imports scan succeeds");
    CHECK_EQ(count_decls(*r.program), 3, "3 decls");
}

// ── Main ─────────────────────────────────────────────────────────────
static void test_import() {
    test_from_simple();
    test_import_simple();
    test_export_behaves_like_import();
    test_from_with_alias();
    test_import_with_alias();
    test_from_with_selective();
    test_from_with_selective_alias();
    test_import_with_selective();
    test_export_with_selective();
    test_from_with_depth();
    test_from_with_unlimited_depth();
    test_from_asset();
    test_from_asset_requires_alias();
    test_relative_import();
    test_from_empty_path();
    test_import_empty_path();
    test_from_selective_empty();
    test_selective_trailing_comma();
    test_import_relative();
    test_export_relative();
    test_multiple_imports();
}

TEST_MAIN(import)
