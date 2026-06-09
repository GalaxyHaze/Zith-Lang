# ADR-0002: SCAN -> SEMA -> SOLVE Pipeline

## Status

Implemented

## Context

The current parser was a monolithic recursive-descent parser that walked tokens and produced
an AST in one pass. To support symbol registration before body parsing (needed for forward
references, imports, generics, and macros), we split into three sequential phases.

## Decision

Replace the monolithic `Parser` with a three-phase pipeline, all orchestrated by
`CompilationSession`. The pipeline runs inside `parseStage()`:

### Architecture

```
CompilationSession (owns tokens, builder, diags, syms, types, program, scan_result)
  │
  ├── parseStage()
  │     ├── scanStage()     → Parser::scan() free function
  │     ├── expandBodiesStage() → Parser::expandBodies()
  │     └── solveStage()    → stub (generics/macros/comptime)
```

### Key design

- **`parser::scan()`** is a free function that borrows a `Parser` reference to use its
  token helpers, but does its own top-level token walking. It:
  - Walks tokens looking for `fn`, `struct`, `import` keywords
  - For `fn`: reads the signature (name + params), skips the body by counting braces,
    creates an `UnbodyNode` placeholder, registers the name in `SymbolTable`
  - For `struct`: reads the name, registers it (body scanning later)
  - For `import`: stubbed (will handle file loading in the future)
  - Returns a `ScanResult` containing `DynArray<ScanEntry>` entries per decl kind

- **`Parser::expandBodies()`** iterates `ScanResult` entries, reads `token_start` from
  each `UnbodyNode`, seeks the token stream cursor to that position, parses the real
  body using `parseBlock()`, and replaces the `UnbodyNode` in-place in the AST builder.

- **`CompilationSession::solveStage()`** is a stub for generics, macros, comptime.

### Key types

**`ScanEntry`** (in `src/parser/scan-result.hpp`):
```cpp
struct ScanEntry {
    std::string_view name;
    memory::Span span;
    ast::ExprId body_node;  // UnbodyNode id in the AST builder
};
```

**`UnbodyNode`** (in `src/ast/ast-nodes.hpp`):
```cpp
struct UnbodyNode {
    memory::Span body_span;
    uint32_t token_start;
    uint32_t token_end;
};
```

### Pipeline wiring

In `CompilationSession::parseStage()`:

```cpp
bool CompilationSession::scanStage() {
    parser::Parser parser(&tokens_, &ast_builder_, &diags_);
    scan_result_ = parser::scan(parser, syms_);
    program_ = std::move(parser.program);
    return !diags_.hasErrors();
}

bool CompilationSession::expandBodiesStage() {
    parser::Parser parser(&tokens_, &ast_builder_, &diags_);
    parser.program = std::move(program_);
    parser.expandBodies(scan_result_);
    program_ = std::move(parser.program);
    return !diags_.hasErrors();
}

bool CompilationSession::solveStage() {
    // TODO: generics, macros, comptime
    return true;
}
```

### Imports (future)

When SCAN encounters an `import` declaration, it will:
1. Fire a thread to load the imported file
2. Run `scan()` on that file with `sema = false` (declarations only, no bodies)
3. Merge the imported symbols into the current symbol table
4. Record the dependency for later linking

## Consequences

- **Forward references work**: All symbols are registered before any body is parsed.
- **SEMA is simple**: It knows exactly where to go (via `ScanResult` entries and
  `UnbodyNode` token offsets) — no need for forward declaration tricks.
- **Imports are lightweight**: Only scan declarations, never bodies.
- **`import::Resolver` is now redundant** — SCAN handles top-level symbol registration.
- **Backward compat**: `parser::parseProgram()` still works (wraps scan+expandBodies).
- **Body parsing is slightly slower** (two passes over tokens), but SCAN is extremely
  fast (no expression trees, just brace counting and name registration).

## Files

| File | Action | Purpose |
|------|--------|---------|
| `src/ast/ast-nodes.hpp` | MODIFY | Added `UnbodyNode` to `ExprNode` variant |
| `src/ast/ast-builder.hpp/.cpp` | MODIFY | Added `unbody()` factory |
| `src/import/symbol-table.hpp/.cpp` | MODIFY | Implemented `declare()`/`lookup()`, added `SymbolData` |
| `src/parser/scan-result.hpp` | NEW | `ScanResult` struct and `ScanEntry` |
| `src/parser/parser.hpp/.cpp` | REFACTOR | Added `scan()` free function + `expandBodies()`, removed `parseTopLevel()` |
| `src/cli/compilation-session.hpp/.cpp` | MODIFY | Added `scanStage()`, `expandBodiesStage()`, `solveStage()` |
| `tests/unit/sema-basics.cpp` | MODIFY | Updated for working `lookup()` |
