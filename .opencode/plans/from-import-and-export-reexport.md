# Plan: `from` Symbol Injection + `export` Re-export

## Problem

1. `from std/io/console;` parses and loads the module but **never injects symbols** into the current scope. You can't use `log` directly.
2. `export std/io/console;` parses but doesn't track or re-export any symbols.
3. ModuleRegistry symbols are registered but function/struct symbols from scanned files are **never populated**.

## Key Decisions

- `export fn` stays as an error (no change to `parse_visibility`)
- `from std/io/console;` → injects **all public symbols** into scope
- `from std/io/console { log };` → selective (plan AST field, implement later)
- `from std/io(..)` → recursive sub-folders (already parsed, just needs symbol injection)
- `export module;` → re-exports all symbols from that module
- Name conflicts → **error**

## Data Flow

```
SCAN phase:
  parse_from_import_decl → scan_import_path_if_allowed → scan_file_and_collect_decls
    → populate ModuleRegistry with symbols
    → record in tls_parse_ctx.from_imports

SEMA phase:
  1. Process imported_decls → register functions in ctx.functions (existing)
  2. Process from_imports → for each module in ModuleRegistry, inject public symbols into ctx.functions/ctx.structs
  3. Process re_exports → same injection + mark as re-exported
  4. Analyze function bodies (existing) — now finds `log` directly in scope
```

## Changes

### 1. Populate ModuleRegistry with symbols when scanning

**File:** `impl/parser/parser_decl.cpp` — `scan_file_and_collect_decls` (line ~674)

After collecting declarations from an imported file, iterate and register each **public** symbol in `ModuleRegistry`:

- Derive module name from file path (e.g., `lib/std/io/console.zith` → `std.io.console`)
- `ZITH_NODE_FUNC_DECL` with `visibility == ZITH_VIS_PUBLIC` → `SymbolEntry(kind=Function)`
- `ZITH_NODE_STRUCT_DECL` with `visibility == ZITH_VIS_PUBLIC` → `SymbolEntry(kind=Struct)`

### 2. Track `from` imports and `export` re-exports

**File:** `impl/parser/parser_context.hpp`

Add:
```cpp
struct FromImportInfo { std::string module_path; int recurse_depth; ZithSourceLoc loc; };
struct ReExportInfo   { std::string module_path; ZithSourceLoc loc; };
```

Add vectors to `ParseContext`:
```cpp
std::vector<FromImportInfo> from_imports;
std::vector<ReExportInfo> re_exports;
```

### 3. Record `from` / `export` during parsing

**File:** `impl/parser/parser_decl.cpp`

- `parse_from_import_decl` (line ~877) → push to `tls_parse_ctx.from_imports` with source location
- `parse_export_decl` (line ~938) → push to `tls_parse_ctx.re_exports` with source location

### 4. Inject symbols during SEMA

**File:** `impl/parser/parser_sema.cpp` — `sema_run` (line ~1101)

After existing `imported_decls` processing, add:

```cpp
// Process `from` imports: inject public symbols into current scope
for each from_import in from_imports:
    module = ModuleRegistry.get(from_import.module_path)
    for each public symbol in module:
        if symbol.name already in ctx.functions/ctx.structs → ERROR (conflict, use from_import.loc)
        if Function → add to ctx.functions[symbol.name]
        if Struct   → add to ctx.structs[symbol.name]

// Process `export` re-exports: same injection
for each re_export in re_exports:
    module = ModuleRegistry.get(re_export.module_path)
    for each public symbol in module:
        inject into ctx.functions or ctx.structs (same as from)
```

### 5. Clear state between parses

**File:** `impl/parser/parser.cpp` (line ~163)

Clear `from_imports` and `re_exports` alongside `imported_decls`:
```cpp
tls_parse_ctx.from_imports.clear();
tls_parse_ctx.re_exports.clear();
```

## Bug Fixes (Discovered During Implementation)

### Fix A: `file_path_to_module_name` doesn't handle absolute paths

**Problem:** When scanning `lib/std/io/console.zith`, the parser receives the full absolute path `C:\Users\...\lib\std\io\console.zith`. The current `file_path_to_module_name` strips `lib/` from the **start** of the string, but absolute paths start with `C:\...`. Result: module registered as `C:\Users\...\lib\std.io.console` but lookup uses `std.io.console`.

**Fix:** Use `path.find("lib/")` to find the prefix anywhere in the path, not just at position 0. Also normalize `\` to `/` first for cross-platform consistency.

```cpp
static std::string file_path_to_module_name(const std::string &file_path) {
    std::string path = file_path;
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".zith") == 0)
        path.erase(path.size() - 5);
    for (char &c : path)
        if (c == '\\') c = '/';
    for (const char *prefix : {"lib/", "src/"}) {
        auto pos = path.find(prefix);
        if (pos != std::string::npos) {
            path.erase(0, pos + strlen(prefix));
            break;
        }
    }
    for (char &c : path)
        if (c == '/') c = '.';
    return path;
}
```

### Fix B: `resolve_import_root_dir` fails on Windows paths

**Problem:** Import roots on Windows use `\` separators (e.g., `C:\...\lib\std`), but `resolve_import_root_dir` uses `strrchr(root_full, '/')` which only finds `/`. Result: `root_name` becomes the full path instead of just `std`, and the comparison `root == root_name` fails. Import scanning never happens.

**Fix:** Check for both `/` and `\` when extracting the root name:

```cpp
static bool resolve_import_root_dir(const Parser *p, const std::string &root,
                                    std::string &root_dir) {
    for (size_t i = 0; i < p->import_root_count; ++i) {
        const char *root_full = p->import_roots[i];
        const char *slash     = strrchr(root_full, '/');
        const char *bslash    = strrchr(root_full, '\\');
        const char *sep       = (bslash && (!slash || bslash > slash)) ? bslash : slash;
        const char *root_name = sep ? sep + 1 : root_full;
        if (root == root_name) {
            root_dir = root_full;
            return true;
        }
    }
    return false;
}
```

### Fix C: Empty source location in diagnostics produces garbled output

**Problem:** `parser_emit_diag(p, {}, ZITH_DIAG_ERROR, buf)` uses an empty `ZithSourceLoc{}` with line=0, column=0. The diagnostic system reads from the wrong position in the source buffer, producing corrupted output like `}   log("Hello");io/console;`.

**Fix:** Store `ZithSourceLoc` in `FromImportInfo` and `ReExportInfo` during parsing, then use it when emitting errors in SEMA.

### Fix D: Empty module name in ModuleRegistry

**Problem:** `Scanned symbols from ModuleRegistry: Module: ` shows a module with an empty name. This is likely caused by a file being scanned with no proper filename, or by the path normalization producing an empty string.

**Fix:** Add a guard in `ensure_parser_module` to skip registration if the normalized module name is empty (already exists, but verify). Also add debug logging to identify the source of the empty module.

## Files to Modify

| File | Change |
|------|--------|
| `impl/parser/parser_context.hpp` | Add `FromImportInfo`, `ReExportInfo` structs with `ZithSourceLoc` and vectors |
| `impl/parser/parser_decl.cpp` | Fix `file_path_to_module_name` for absolute paths; Fix `resolve_import_root_dir` for Windows; Record from/export with source location |
| `impl/parser/parser_sema.cpp` | Inject from/export symbols into sema scope; Use stored source locations for errors |
| `impl/parser/parser.cpp` | Clear new state between parses; Add `get_tls_parse_ctx()` accessor |

## What's NOT in scope

- `()` recursion specifier for `from` — already parsed, just pass depth through
- `{ sym1, sym2 }` selective import — plan the AST field but don't implement yet
- `export fn` visibility modifier — stays as error
