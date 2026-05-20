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
struct FromImportInfo { std::string module_path; int recurse_depth; };
struct ReExportInfo   { std::string module_path; };
```

Add vectors to `ParseContext`:
```cpp
std::vector<FromImportInfo> from_imports;
std::vector<ReExportInfo> re_exports;
```

### 3. Record `from` / `export` during parsing

**File:** `impl/parser/parser_decl.cpp`

- `parse_from_import_decl` (line ~877) → push to `tls_parse_ctx.from_imports`
- `parse_export_decl` (line ~938) → push to `tls_parse_ctx.re_exports`

### 4. Inject symbols during SEMA

**File:** `impl/parser/parser_sema.cpp` — `sema_run` (line ~1101)

After existing `imported_decls` processing, add:

```cpp
// Process `from` imports: inject public symbols into current scope
for each from_import in from_imports:
    module = ModuleRegistry.get(from_import.module_path)
    for each public symbol in module:
        if symbol.name already in ctx.functions/ctx.structs → ERROR (conflict)
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

## Files to Modify

| File | Change |
|------|--------|
| `impl/parser/parser_context.hpp` | Add `FromImportInfo`, `ReExportInfo` structs and vectors |
| `impl/parser/parser_decl.cpp` | Record from/export in ParseContext; populate ModuleRegistry symbols |
| `impl/parser/parser_sema.cpp` | Inject from/export symbols into sema scope |
| `impl/parser/parser.cpp` | Clear new state between parses |

## What's NOT in scope

- `()` recursion specifier for `from` — already parsed, just pass depth through
- `{ sym1, sym2 }` selective import — plan the AST field but don't implement yet
- `export fn` visibility modifier — stays as error
