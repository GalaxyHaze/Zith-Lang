#include "ide/lsp-ide-api.hpp"
#include "test-common.hpp"

#include <string_view>

void test_ide_api() {
    zith::ide::WorkspaceConfig config;
    config.workspaceRoot = "/tmp/zith-ide-tests";
    config.stdlibPath = ZITH_STDLIB_DIR;

    zith::ide::Workspace workspace(config);
    const auto snapshot =
        workspace.openDocument("file:///tmp/main.zith", "/tmp/main.zith",
                               "struct Counter { value: i32 }\n"
                               "fn bump(c: view Counter): i32 { c.value + 1 }\n"
                               "state Worker(): i32 { return 42; }\n"
                               "fn main(): i32 { return dock Worker(); }\n",
                               1);

    zith::ide::Query query;
    const auto result = workspace.analyze(snapshot, query);

    CHECK(result.kind == zith::ide::QueryKind::Diagnostics,
          "diagnostics query is preserved in the result");
    CHECK(result.ok, "well-formed snapshot analyzes without errors");
    CHECK_EQ(result.analyzedRevision, snapshot.revision,
             "analyzed revision matches the in-memory document revision");
    for (const auto &diag : result.diagnostics) {
        std::printf("  IDE diagnostic: %s\n", diag.message.c_str());
    }
    CHECK(result.diagnostics.empty(), "open document has no diagnostics");
    CHECK_EQ(result.error, std::string(), "no error text is reported");

    zith::ide::Query symbols_query;
    symbols_query.kind = zith::ide::QueryKind::DocumentSymbols;
    const auto symbols = workspace.analyze(snapshot, symbols_query);
    CHECK(symbols.ok, "document symbols query succeeds");
    CHECK(!symbols.documentSymbols.empty(), "document symbols are collected");

    zith::ide::Query completion_query;
    completion_query.kind = zith::ide::QueryKind::Completion;
    completion_query.line = 3;
    completion_query.character = 1;
    const auto completions = workspace.analyze(snapshot, completion_query);
    CHECK(completions.ok, "completion query succeeds");
    CHECK(!completions.completions.empty(), "completion items are collected");
    bool saw_keyword = false;
    for (const auto &item : completions.completions) {
        if (item.label == "state") {
            saw_keyword = true;
            break;
        }
    }
    CHECK(saw_keyword, "state keyword is exposed by the facade");
}

TEST_MAIN(ide_api)
