#include "ast/ast-builder.hpp"
#include "ast/ast-ids.hpp"
#include "ast/ast-nodes.hpp"
#include "ast/ast-printer.hpp"
#include "diagnostics/diagnostic-engine.hpp"
#include "diagnostics/error-codes.hpp"
#include "import/import-manager.hpp"
#include "memory/arena.hpp"
#include "memory/dyn-array.hpp"
#include "memory/source-map.hpp"
#include "support/platform.hpp"

#include "../test-common.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

// ── diagnostic assertion helpers ──────────────────────────────────

#define CHECK_DIAG_COUNT(diags, n)                                                                 \
    do {                                                                                           \
        auto _all = (diags).all();                                                                 \
        CHECK_EQ(_all.size(), size_t(n), "expected " #n " diagnostic(s)");                         \
    } while (0)

#define CHECK_DIAG_AT(diags, i, sev, errc)                                                         \
    do {                                                                                           \
        auto _all = (diags).all();                                                                 \
        CHECK(i < _all.size(), "diagnostic index " #i " in range");                                \
        if (i < _all.size()) {                                                                     \
            CHECK_EQ(static_cast<int>(_all[i].severity),                                           \
                     static_cast<int>(zith::diagnostics::Severity::sev),                           \
                     "diagnostic[" #i "] severity is " #sev);                                      \
            CHECK_EQ(_all[i].code, zith::diagnostics::err::errc,                                   \
                     "diagnostic[" #i "] code is " #errc);                                         \
        }                                                                                          \
    } while (0)

namespace fs = std::filesystem;

static struct Cleanup {
    std::vector<std::string> dirs;
    ~Cleanup() {
        for (auto &d : dirs) {
            std::filesystem::remove_all(d);
        }
    }
} cleanup;

using namespace zith::ast;
using namespace zith::import;
using zith::memory::Arena;
using zith::memory::DynArray;

static std::string make_tmp_dir() {
    auto base = fs::temp_directory_path();
    std::string tmpl = (base / "zith_e2e_XXXXXX").string();
    char *d = zith::support::mkdtemp(tmpl.data());
    if (d)
        cleanup.dirs.push_back(d);
    return d ? std::string(d) : std::string{};
}
static auto source_map = zith::memory::SourceMap();

// ── test 1: import (default) → merged with prefix ──────────────

static void test_import_with_prefix() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/mymod.zith");
        f << "pub fn greet() {}\n";
        f << "mod fn util() {}\n";
        f << "fn hidden() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("mymod");
    auto res = mgr.resolve(path, /*is_from=*/false);
    CHECK(res.isOk(), "resolve mymod");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== import (prefix) ===\n");
    main_syms.dump();

    CHECK(main_syms.lookup("mymod.greet") != kInvalidSym, "mymod.greet found (pub)");
    CHECK(main_syms.lookup("mymod.util") != kInvalidSym, "mymod.util found (mod(1) at depth 1)");
    CHECK(main_syms.lookup("mymod.hidden") == kInvalidSym, "mymod.hidden NOT found (private)");
    CHECK(main_syms.lookup("greet") == kInvalidSym, "bare greet NOT found (import uses prefix)");
}

// ── test 2: from → bare + last-segment prefix ────────────────────

static void test_from_dual_registration() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/frommod.zith");
        f << "pub fn greet() {}\n";
        f << "mod fn util() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("frommod");
    auto res = mgr.resolve(path, /*is_from=*/true);
    CHECK(res.isOk(), "resolve frommod");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== from (bare + last-segment prefix) ===\n");
    main_syms.dump();

    // from registers BOTH bare and last-segment prefixed
    CHECK(main_syms.lookup("greet") != kInvalidSym, "bare greet found (from)");
    CHECK(main_syms.lookup("frommod.greet") != kInvalidSym,
          "frommod.greet also found (last-segment prefix)");
    CHECK(main_syms.lookup("util") != kInvalidSym, "bare util found (mod(1) at depth 1)");
    CHECK(main_syms.lookup("frommod.util") != kInvalidSym,
          "frommod.util also found (last-segment prefix)");
}

// ── test 3: mod(1) blocked at depth 2 ───────────────────────────

static void test_depth_blocks_at_distance() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/depthmod.zith");
        f << "pub fn always_visible() {}\n";
        f << "mod(1) fn limited() {}\n";
        f << "mod(3) fn deep_ok() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("depthmod");
    auto res = mgr.resolve(path);
    CHECK(res.isOk(), "resolve depthmod");
    if (!res.isOk())
        return;

    // merge at depth 2 → mod(1) should be blocked, pub and mod(3) pass
    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms, /*from_depth=*/2);

    std::printf("=== depth check (from_depth=2) ===\n");
    std::printf("  mod(1) limited → should be BLOCKED\n");
    std::printf("  mod(3) deep_ok → should PASS\n");
    std::printf("  pub always_visible → should PASS\n");
    main_syms.dump();

    CHECK(main_syms.lookup("depthmod.always_visible") != kInvalidSym, "pub always passes");
    CHECK(main_syms.lookup("depthmod.limited") == kInvalidSym, "mod(1) blocked at depth 2");
    CHECK(main_syms.lookup("depthmod.deep_ok") != kInvalidSym, "mod(3) passes at depth 2");
}

// ── test 4: from + depth — dual-registration respects depth ─────

static void test_from_depth_blocks() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/fdepth.zith");
        f << "mod(1) fn shallow() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("fdepth");
    auto res = mgr.resolve(path, /*is_from=*/true);
    CHECK(res.isOk(), "resolve fdepth");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms, /*from_depth=*/2);

    std::printf("=== from + depth check (from_depth=2) ===\n");
    main_syms.dump();

    // Both bare and last-segment versions should be blocked
    CHECK(main_syms.lookup("shallow") == kInvalidSym, "bare shallow blocked (mod(1), depth 2)");
    CHECK(main_syms.lookup("fdepth.shallow") == kInvalidSym,
          "fdepth.shallow blocked (mod(1), depth 2)");
}

// ── test 5: import alias ─────────────────────────────────────────

static void test_import_alias() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        fs::create_directories(tmp_dir + "/some/long");
        std::ofstream f(tmp_dir + "/some/long/path.zith");
        f << "pub fn hello() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("some");
    path.push("long");
    path.push("path");
    auto res = mgr.resolve(path, /*is_from=*/false, /*is_export=*/false, "short");
    CHECK(res.isOk(), "resolve some/long/path with alias");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== import alias ===\n");
    main_syms.dump();

    CHECK(main_syms.lookup("short.hello") != kInvalidSym, "short.hello found (alias)");
    CHECK(main_syms.lookup("some.long.path.hello") == kInvalidSym,
          "full prefix NOT found (alias used)");
}

// ── test 6: cycle detection ──────────────────────────────────────

static void test_cycle_detection() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/a.zith");
        f << "import b\n";
    }
    {
        std::ofstream f(tmp_dir + "/b.zith");
        f << "import a\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path_a(arena);
    path_a.push("a");
    // The import system doesn't do transitive resolution yet,
    // so cycles can only happen if the same import key is re-resolved.
    // Direct cycle test via resolve's `resolving_` set.
    // First resolve a:
    auto res_a = mgr.resolve(path_a);
    CHECK(res_a.isOk(), "resolve a (first)");
    if (!res_a.isOk())
        return;

    // Re-resolve a should return the cached index (dedup, not cycle)
    auto res_a2 = mgr.resolve(path_a);
    CHECK(res_a2.isOk(), "resolve a (second, cached)");
}

// ── test 7: not visible (outside visible roots) ──────────────────

static void test_not_visible() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/hidden.zith");
        f << "pub fn secret() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    // Empty visible roots — file won't be found
    ImportManager mgr(arena, source_map, diags, {});

    DynArray<std::string_view> path(arena);
    path.push("hidden");
    auto res = mgr.resolve(path);
    CHECK(!res.isOk(), "resolve hidden should fail (not in visible roots)");
}

// ── test 8: mod.zith directory lookup ────────────────────────────

static void test_mod_dir_lookup() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/mymod");
    {
        std::ofstream f(tmp_dir + "/mymod/mod.zith");
        f << "pub fn from_mod_dir() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("mymod");
    auto res = mgr.resolve(path);
    CHECK(res.isOk(), "resolve mymod (mod.zith dir)");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    CHECK(main_syms.lookup("mymod.from_mod_dir") != kInvalidSym, "mymod.from_mod_dir found");
}

// ── test 9: hatch escape (../ within root) ───────────────────────

static void test_hatch_escape() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/src");
    {
        std::ofstream f(tmp_dir + "/lib.zith");
        f << "pub fn lib_fn() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("..");
    path.push("lib");
    auto res = mgr.resolve(path, /*is_from=*/false, /*is_export=*/false,
                           /*alias=*/{}, /*import_depth=*/1, tmp_dir + "/src/main.zith");
    CHECK(res.isOk(), "resolve ../lib from src/main.zith");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    CHECK(main_syms.lookup("lib.lib_fn") != kInvalidSym, "lib.lib_fn found via ../");
}

// ── test 10: hatch escape blocked (../ beyond root) ──────────────

static void test_hatch_escape_blocked() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/sub");
    {
        std::ofstream f(tmp_dir + "/sub/x.zith");
        f << "pub fn x() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("..");
    path.push("..");
    path.push("outside");
    // Resolves to /tmp/outside.zith which is outside tmp_dir
    auto res = mgr.resolve(path, /*is_from=*/false, /*is_export=*/false,
                           /*alias=*/{}, /*import_depth=*/1, tmp_dir + "/sub/x.zith");
    CHECK(!res.isOk(), "resolve ../../outside should fail (beyond visible root)");
}

// ── test 11: from with multi-segment path (last-segment prefix) ───

static void test_from_multi_segment() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/sub/dir");
    {
        std::ofstream f(tmp_dir + "/sub/dir/module.zith");
        f << "pub fn multi() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("sub");
    path.push("dir");
    path.push("module");
    auto res = mgr.resolve(path, /*is_from=*/true);
    CHECK(res.isOk(), "resolve sub/dir/module (from)");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== from multi-segment ===\n");
    main_syms.dump();

    // Last segment is "module"
    CHECK(main_syms.lookup("multi") != kInvalidSym, "bare multi found (from)");
    CHECK(main_syms.lookup("module.multi") != kInvalidSym,
          "module.multi found (last-segment prefix)");
    // Full path prefix should NOT appear with from
    CHECK(main_syms.lookup("sub.dir.module.multi") == kInvalidSym, "full prefix NOT used (from)");
}

// ── test 12: export (is_export flag threaded correctly) ──────────

static void test_export_simple() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/exported.zith");
        f << "pub fn from_export() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("exported");
    auto res = mgr.resolve(path, /*is_from=*/true, /*is_export=*/true);
    CHECK(res.isOk(), "resolve exported (export)");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    // Export behaves like from: bare + last-segment prefix
    CHECK(main_syms.lookup("from_export") != kInvalidSym, "bare from_export found (export)");
    CHECK(main_syms.lookup("exported.from_export") != kInvalidSym,
          "exported.from_export found (last-segment)");
}

// ── test 13: multiple different imports ─────────────────────────

static void test_multiple_imports() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/a.zith");
        f << "pub fn fn_a() {}\n";
    }
    {
        std::ofstream f(tmp_dir + "/b.zith");
        f << "pub fn fn_b() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path_a(arena);
    path_a.push("a");
    auto res_a = mgr.resolve(path_a);
    CHECK(res_a.isOk(), "resolve a");
    if (!res_a.isOk())
        return;

    DynArray<std::string_view> path_b(arena);
    path_b.push("b");
    auto res_b = mgr.resolve(path_b);
    CHECK(res_b.isOk(), "resolve b");
    if (!res_b.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    CHECK(main_syms.lookup("a.fn_a") != kInvalidSym, "a.fn_a found");
    CHECK(main_syms.lookup("b.fn_b") != kInvalidSym, "b.fn_b found");
}

// ── test 14: import with ../ (prefix naming) ────────────────────

static void test_import_dotdot_prefix() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/nested");
    {
        std::ofstream f(tmp_dir + "/outer.zith");
        f << "pub fn outer_fn() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("..");
    path.push("outer");
    auto res = mgr.resolve(path, /*is_from=*/false, /*is_export=*/false,
                           /*alias=*/{}, /*import_depth=*/1, tmp_dir + "/nested/any.zith");
    CHECK(res.isOk(), "resolve ../outer from nested/any.zith");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    // import uses prefix: outer.outer_fn
    CHECK(main_syms.lookup("outer.outer_fn") != kInvalidSym, "outer.outer_fn found (import ../)");
}

// ── test 15: directory import with depth limit (1 level) ──────────

static void test_dir_depth_limited() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/mydir/sub");
    {
        std::ofstream f(tmp_dir + "/mydir/a.zith");
        f << "pub fn top_fn() {}\n";
    }
    {
        std::ofstream f(tmp_dir + "/mydir/sub/b.zith");
        f << "pub fn deep_fn() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("mydir");
    // Default import_depth=1 — immediate children only
    auto res = mgr.resolve(path, /*is_from=*/false, /*is_export=*/false,
                           /*alias=*/{}, /*import_depth=*/1);
    CHECK(res.isOk(), "resolve mydir (depth=1)");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== dir depth=1 ===\n");
    main_syms.dump();

    CHECK(main_syms.lookup("mydir.a.top_fn") != kInvalidSym, "mydir.a.top_fn found (depth 1)");
    CHECK(main_syms.lookup("mydir.sub.b.deep_fn") == kInvalidSym,
          "mydir.sub.b.deep_fn NOT found (depth >1)");
}

// ── test 16: directory import with infinite depth (..) ────────────

static void test_dir_depth_infinite() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/mydir/sub");
    {
        std::ofstream f(tmp_dir + "/mydir/a.zith");
        f << "pub fn top_fn() {}\n";
    }
    {
        std::ofstream f(tmp_dir + "/mydir/sub/b.zith");
        f << "pub fn deep_fn() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("mydir");
    auto res = mgr.resolve(path, /*is_from=*/false, /*is_export=*/false,
                           /*alias=*/{}, /*import_depth=*/-1);
    CHECK(res.isOk(), "resolve mydir (depth=..)");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== dir depth=.. ===\n");
    main_syms.dump();

    CHECK(main_syms.lookup("mydir.a.top_fn") != kInvalidSym, "mydir.a.top_fn found");
    CHECK(main_syms.lookup("mydir.sub.b.deep_fn") != kInvalidSym,
          "mydir.sub.b.deep_fn found (infinite depth)");
}

// ── test 17: from with directory depth (..) ───────────────────────

static void test_from_dir_depth_infinite() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    fs::create_directories(tmp_dir + "/mydir/sub");
    {
        std::ofstream f(tmp_dir + "/mydir/a.zith");
        f << "pub fn top_fn() {}\n";
    }
    {
        std::ofstream f(tmp_dir + "/mydir/sub/b.zith");
        f << "pub fn deep_fn() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("mydir");
    auto res = mgr.resolve(path, /*is_from=*/true, /*is_export=*/false,
                           /*alias=*/{}, /*import_depth=*/-1);
    CHECK(res.isOk(), "resolve mydir (from, depth=..)");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== from dir depth=.. ===\n");
    main_syms.dump();

    // from strips prefix: bare names + last-segment prefix
    CHECK(main_syms.lookup("top_fn") != kInvalidSym, "bare top_fn found");
    CHECK(main_syms.lookup("a.top_fn") != kInvalidSym, "a.top_fn found (dir entry prefix)");
    CHECK(main_syms.lookup("deep_fn") != kInvalidSym, "bare deep_fn found");
    CHECK(main_syms.lookup("b.deep_fn") != kInvalidSym, "b.deep_fn found (dir entry prefix)");
}

// ── test 18: export with directory (re-export all symbols) ────────

static void test_export_directory() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/inner.zith");
        f << "pub fn inner_fn() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("inner");
    // Resolve as export (is_export=true, is_from=true)
    auto res = mgr.resolve(path, /*is_from=*/true, /*is_export=*/true);
    CHECK(res.isOk(), "resolve inner (export)");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== export directory ===\n");
    main_syms.dump();

    // export behaves like from: bare + last-segment prefix
    CHECK(main_syms.lookup("inner_fn") != kInvalidSym, "bare inner_fn found (export)");
    CHECK(main_syms.lookup("inner.inner_fn") != kInvalidSym,
          "inner.inner_fn found (last-segment prefix)");
}

// ── test 19: cycle detection via re-export chain ─────────────────

static void test_cycle_via_re_export() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/acyc.zith");
        f << "pub fn a_fn() {}\n";
        f << "export bcyc\n";
    }
    {
        std::ofstream f(tmp_dir + "/bcyc.zith");
        f << "pub fn b_fn() {}\n";
        f << "export acyc\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("acyc");
    auto res = mgr.resolve(path, /*is_from=*/false);
    CHECK(res.isOk(), "resolve acyc succeeds despite cycle in re-export");

    // Should have a warning about the cyclic re-export
    CHECK(diags.hasErrors() == false, "cycle re-export is warning, not error");
    CHECK_DIAG_COUNT(diags, 1);
    CHECK_DIAG_AT(diags, 0, Warning, ImportError);

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== cycle via re-export ===\n");
    main_syms.dump();

    // acyc's own symbols should be present
    CHECK(main_syms.lookup("acyc.a_fn") != kInvalidSym, "acyc.a_fn found (own symbol)");
    // bcyc was resolved successfully (its re-export of acyc failed, not bcyc itself)
    // so bcyc's symbols ARE re-exported through acyc
    CHECK(main_syms.lookup("acyc.b_fn") != kInvalidSym, "acyc.b_fn found (re-exported from bcyc)");
    // bcyc also has its own entry
    CHECK(main_syms.lookup("bcyc.b_fn") != kInvalidSym, "bcyc.b_fn found (own entry)");
    // No circular re-export of acyc through bcyc (that re-export failed)
    CHECK(main_syms.lookup("bcyc.a_fn") == kInvalidSym,
          "bcyc.a_fn NOT found (acyc re-export through bcyc failed)");
}

// ── test 20: re-export chain (A → B → C) ──────────────────────────

static void test_re_export_chain() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/cchain.zith");
        f << "pub fn deep() {}\n";
    }
    {
        std::ofstream f(tmp_dir + "/bchain.zith");
        f << "pub fn b_mid() {}\n";
        f << "export cchain\n";
    }
    {
        std::ofstream f(tmp_dir + "/achain.zith");
        f << "pub fn a_top() {}\n";
        f << "export bchain\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("achain");
    auto res = mgr.resolve(path);
    CHECK(res.isOk(), "resolve achain");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== re-export chain (A→B→C) ===\n");
    main_syms.dump();

    // C's bare "deep" is registered by C's own entry, then B's re-export
    // of C tries to register the same bare name → 1 DuplicateDecl diagnostic
    // (duplicate is skipped, so deep appears once via C's own entry)
    CHECK_DIAG_COUNT(diags, 1);
    CHECK_DIAG_AT(diags, 0, Error, DuplicateDecl);

    // achain's own symbol
    CHECK(main_syms.lookup("achain.a_top") != kInvalidSym, "achain.a_top found (own)");
    // bchain's symbol re-exported through achain
    CHECK(main_syms.lookup("achain.b_mid") != kInvalidSym,
          "achain.b_mid found (re-exported from B)");
    // cchain's symbol re-exported through achain (transitive)
    CHECK(main_syms.lookup("achain.deep") != kInvalidSym,
          "achain.deep found (re-exported from C via B)");
    // bchain's symbol also registered under its own prefix
    CHECK(main_syms.lookup("bchain.b_mid") != kInvalidSym, "bchain.b_mid found (own entry)");
    // cchain's symbol also registered under its own prefix
    CHECK(main_syms.lookup("cchain.deep") != kInvalidSym, "cchain.deep found (own entry)");
}

// ── test 21: duplicate public symbol detection ────────────────────

static void test_duplicate_public_symbols() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/dupa.zith");
        f << "pub fn conflict() {}\n";
    }
    {
        std::ofstream f(tmp_dir + "/dupb.zith");
        f << "pub fn conflict() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    // Resolve both files with `from` (bare name registration)
    DynArray<std::string_view> path_a(arena);
    path_a.push("dupa");
    auto res_a = mgr.resolve(path_a, /*is_from=*/true);
    CHECK(res_a.isOk(), "resolve dupa (from)");
    if (!res_a.isOk())
        return;

    DynArray<std::string_view> path_b(arena);
    path_b.push("dupb");
    auto res_b = mgr.resolve(path_b, /*is_from=*/true);
    CHECK(res_b.isOk(), "resolve dupb (from)");
    if (!res_b.isOk())
        return;

    CHECK_DIAG_COUNT(diags, 0);

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    std::printf("=== duplicate public symbols ===\n");
    main_syms.dump();

    // Bare "conflict" from dupa declared first; dupb's bare "conflict" triggers DuplicateDecl
    // The last-segment prefixed names (dupa.conflict vs dupb.conflict) are different → no conflict
    CHECK(diags.hasErrors(), "duplicate symbol error reported");
    CHECK_DIAG_COUNT(diags, 1);
    CHECK_DIAG_AT(diags, 0, Error, DuplicateDecl);
}

// ── test 22: isLoaded and get API ─────────────────────────────────

static void test_isLoaded_and_get() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/apiz.zith");
        f << "pub fn api_fn() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    CHECK(!mgr.isLoaded("apiz"), "isLoaded false before resolve");

    DynArray<std::string_view> path(arena);
    path.push("apiz");
    auto res = mgr.resolve(path);
    CHECK(res.isOk(), "resolve apiz");
    if (!res.isOk())
        return;

    CHECK(mgr.isLoaded("apiz"), "isLoaded true after resolve");
    CHECK(mgr.isLoaded("nonexistent") == false, "isLoaded false for unknown path");

    auto &file = mgr.get(res.value());
    CHECK_EQ(file.import_key, std::string("apiz"), "loaded file import_key matches");
    CHECK_EQ(file.is_from, false, "loaded file is_from matches");
    CHECK_EQ(file.is_export, false, "loaded file is_export matches");
    CHECK_EQ(file.import_depth, int32_t(1), "loaded file import_depth matches");
    CHECK(file.file_id != 0, "loaded file has valid file_id");
}

// ── test 23: resolution failure diagnostics ───────────────────────

static void test_resolution_failure_diagnostics() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    zith::diagnostics::DiagnosticEngine diags(arena);
    // Empty visible roots — nothing can be resolved
    ImportManager mgr(arena, source_map, diags, {});

    DynArray<std::string_view> path(arena);
    path.push("nope");
    auto res = mgr.resolve(path);
    CHECK(!res.isOk(), "resolve nope fails");

    // Resolution failure returns error but does NOT report diagnostic
    // (caller is responsible for reporting)
    CHECK_DIAG_COUNT(diags, 0);
}

// ── test 24: import empty file (no symbols) ───────────────────────

static void test_import_empty_file() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/empty.zith");
        f << "\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    DynArray<std::string_view> path(arena);
    path.push("empty");
    auto res = mgr.resolve(path);
    CHECK(res.isOk(), "resolve empty file");
    if (!res.isOk())
        return;

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    CHECK_EQ(main_syms.symbolCount(), size_t(0), "no symbols from empty file");
}

// ── test 25: import key dedup preserves first config ──────────────

static void test_import_dedup_preserves_first_config() {
    Arena arena;
    auto tmp_dir = make_tmp_dir();
    CHECK(!tmp_dir.empty(), "tmp dir created");
    if (tmp_dir.empty())
        return;

    {
        std::ofstream f(tmp_dir + "/shared.zith");
        f << "pub fn x() {}\n";
    }

    zith::diagnostics::DiagnosticEngine diags(arena);
    ImportManager mgr(arena, source_map, diags, {tmp_dir});

    // First resolve as import (prefix)
    DynArray<std::string_view> path(arena);
    path.push("shared");
    auto res1 = mgr.resolve(path, /*is_from=*/false);
    CHECK(res1.isOk(), "first resolve (import)");
    if (!res1.isOk())
        return;

    // Second resolve as from — should return cached (first config wins)
    DynArray<std::string_view> path2(arena);
    path2.push("shared");
    auto res2 = mgr.resolve(path2, /*is_from=*/true);
    CHECK(res2.isOk(), "second resolve (from) returns cached");
    CHECK_EQ(res1.value(), res2.value(), "same index returned");

    SymbolTable main_syms(arena);
    mgr.mergeInto(main_syms);

    // First config (import/prefix) should be used
    CHECK(main_syms.lookup("shared.x") != kInvalidSym, "shared.x found (prefix from first config)");
    // Bare name should NOT be registered (first config was import, not from)
    CHECK(main_syms.lookup("x") == kInvalidSym, "bare x NOT found (first config was import)");
}

int main() {
    std::printf("import-e2e tests\n");
    std::printf("===================\n\n");

    test_import_with_prefix();
    test_from_dual_registration();
    test_depth_blocks_at_distance();
    test_from_depth_blocks();
    test_import_alias();
    test_cycle_detection();
    test_not_visible();
    test_mod_dir_lookup();
    test_hatch_escape();
    test_hatch_escape_blocked();
    test_from_multi_segment();
    test_export_simple();
    test_multiple_imports();
    test_import_dotdot_prefix();
    test_dir_depth_limited();
    test_dir_depth_infinite();
    test_from_dir_depth_infinite();
    test_export_directory();
    test_cycle_via_re_export();
    test_re_export_chain();
    test_duplicate_public_symbols();
    test_isLoaded_and_get();
    test_resolution_failure_diagnostics();
    test_import_empty_file();
    test_import_dedup_preserves_first_config();

    std::printf("\nResults: %d passed, %d failed\n", g_test_passed, g_test_failed);
    return g_test_failed > 0 ? 1 : 0;
}
