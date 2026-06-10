#include "import/import-manager.hpp"
#include "ast/ast-printer.hpp"
#include "ast/ast-builder.hpp"
#include "ast/ast-nodes.hpp"
#include "ast/ast-ids.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "cli/options.hpp"
#include "diagnostics/diagnostic-engine.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static int failed = 0;
static int passed = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); failed++; } \
    else { std::printf("  PASS: %s\n", msg); passed++; } \
} while(0)

#define CHECK_EQ(a, b, msg) CHECK((a) == (b), msg)

namespace fs = std::filesystem;

static struct Cleanup {
    std::vector<std::string> dirs;
    ~Cleanup() { for (auto &d : dirs) { std::filesystem::remove_all(d); } }
} cleanup;

using namespace zith::ast;
using namespace zith::import;
using zith::memory::Arena;
using zith::memory::DynArray;

static std::string make_tmp_dir() {
    char tmpl[] = "/tmp/zith_e2e_XXXXXX";
    const char *d = mkdtemp(tmpl);
    if (d) cleanup.dirs.push_back(d);
    return d ? std::string(d) : std::string{};
}

// ── test 1: import (default) → merged with prefix ──────────────

static void test_import_with_prefix() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty()) return;

    {
        std::ofstream f(tmp_dir + "/mymod.zith");
        f << "pub fn greet() {}\n";
        f << "mod fn util() {}\n";
        f << "fn hidden() {}\n";
    }

    zith::cli::Options opts;
    opts.include_dirs.push(tmp_dir);
    zith::diagnostics::DiagnosticEngine diags;
    ImportManager mgr(arena, opts, diags);

    DynArray<std::string_view> path(arena);
    path.push("mymod");
    auto res = mgr.resolve(path, /*is_from=*/false);
    CHECK(res.isOk(), "resolve mymod");
    if (!res.isOk()) return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== import (prefix) ===\n");
    main_syms.dump();

    CHECK(main_syms.lookup("mymod.greet") != kInvalidSym, "mymod.greet found (pub)");
    CHECK(main_syms.lookup("mymod.util") != kInvalidSym,   "mymod.util found (mod(1) at depth 1)");
    CHECK(main_syms.lookup("mymod.hidden") == kInvalidSym,  "mymod.hidden NOT found (private)");
    CHECK(main_syms.lookup("greet") == kInvalidSym,         "bare greet NOT found (import uses prefix)");
}

// ── test 2: from → merged WITHOUT prefix ────────────────────────

static void test_from_bare_names() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty()) return;

    {
        std::ofstream f(tmp_dir + "/frommod.zith");
        f << "pub fn greet() {}\n";
        f << "mod fn util() {}\n";
    }

    zith::cli::Options opts;
    opts.include_dirs.push(tmp_dir);
    zith::diagnostics::DiagnosticEngine diags;
    ImportManager mgr(arena, opts, diags);

    DynArray<std::string_view> path(arena);
    path.push("frommod");
    auto res = mgr.resolve(path, /*is_from=*/true);
    CHECK(res.isOk(), "resolve frommod");
    if (!res.isOk()) return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== from (bare names) ===\n");
    main_syms.dump();

    CHECK(main_syms.lookup("greet") != kInvalidSym,         "bare greet found (from)");
    CHECK(main_syms.lookup("util") != kInvalidSym,           "bare util found (mod(1) at depth 1)");
    CHECK(main_syms.lookup("frommod.greet") == kInvalidSym,  "prefixed greet NOT found (from uses bare)");
}

// ── test 3: mod(1) blocked at depth 2 ───────────────────────────

static void test_depth_blocks_at_distance() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty()) return;

    {
        std::ofstream f(tmp_dir + "/depthmod.zith");
        f << "pub fn always_visible() {}\n";
        f << "mod(1) fn limited() {}\n";
        f << "mod(3) fn deep_ok() {}\n";
    }

    zith::cli::Options opts;
    opts.include_dirs.push(tmp_dir);
    zith::diagnostics::DiagnosticEngine diags;
    ImportManager mgr(arena, opts, diags);

    DynArray<std::string_view> path(arena);
    path.push("depthmod");
    auto res = mgr.resolve(path);
    CHECK(res.isOk(), "resolve depthmod");
    if (!res.isOk()) return;

    // merge at depth 2 → mod(1) should be blocked, pub and mod(3) pass
    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms, /*from_depth=*/2);

    std::printf("=== depth check (from_depth=2) ===\n");
    std::printf("  mod(1) limited → should be BLOCKED\n");
    std::printf("  mod(3) deep_ok → should PASS\n");
    std::printf("  pub always_visible → should PASS\n");
    main_syms.dump();

    CHECK(main_syms.lookup("depthmod.always_visible") != kInvalidSym, "pub always passes");
    CHECK(main_syms.lookup("depthmod.limited") == kInvalidSym,        "mod(1) blocked at depth 2");
    CHECK(main_syms.lookup("depthmod.deep_ok") != kInvalidSym,        "mod(3) passes at depth 2");
}

// ── test 4: from + depth — from still blocked by mod(N) ─────────

static void test_from_depth_blocks() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty()) return;

    {
        std::ofstream f(tmp_dir + "/fdepth.zith");
        f << "mod(1) fn shallow() {}\n";
    }

    zith::cli::Options opts;
    opts.include_dirs.push(tmp_dir);
    zith::diagnostics::DiagnosticEngine diags;
    ImportManager mgr(arena, opts, diags);

    DynArray<std::string_view> path(arena);
    path.push("fdepth");
    auto res = mgr.resolve(path, /*is_from=*/true);
    CHECK(res.isOk(), "resolve fdepth");
    if (!res.isOk()) return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms, /*from_depth=*/2);

    std::printf("=== from + depth check (from_depth=2) ===\n");
    main_syms.dump();

    CHECK(main_syms.lookup("shallow") == kInvalidSym, "mod(1) shallow blocked (bare, depth 2)");
}

int main() {
    std::printf("import-e2e tests\n");
    std::printf("===================\n\n");

    test_import_with_prefix();
    test_from_bare_names();
    test_depth_blocks_at_distance();
    test_from_depth_blocks();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
