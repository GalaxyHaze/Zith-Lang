# ADR-0002: SCAN -> SEMA -> SOLVE Pipeline

## Status

Proposed

## Context

The current parser is a monolithic recursive-descent parser that walks tokens and produces an
AST in one pass. To support symbol registration before body parsing (needed for forward
references, generics, and macros), we split the parser into three sequential phases.

## Decision

Replace the monolithic `Parser` with three separate stages, all orchestrated by
`CompilationSession` (which already owns all compiler state):

```
CompilationSession (owns tokens, builder, diags, syms, types, program)
  │
  ├── Scanner{tokens, builder, diags, syms}.scanTopLevel(program)
  │     registers symbols, creates UnbodyNode placeholders for fn bodies
  │
  ├── Parser{tokens, builder, diags}.parseProgram(program)
  │     walks program, expands UnbodyNodes by seeking token stream
  │
  └── Solver{syms, types, diags}.solve(program)
  │     generics / macros / comptime (stub)
```

### Stage responsibilities

**SCAN** (`src/parser/scanner.hpp/.cpp`)
- Walks the token stream looking for top-level declarations (`fn`, `struct`, `import`)
- Reads declaration signatures (name, params, field names)
- Registers names in the symbol table via `SymbolTable::declare()`
- Skips function bodies by counting braces, records body span as an `UnbodyNode`
- Produces a `ProgramNode` with `UnbodyNode` placeholders for all function bodies

**SEMA** (`src/parser/parser.hpp/.cpp`, refactored)
- Receives SCAN's `ProgramNode`
- Walks declarations; for each `FnDeclNode` whose body is an `UnbodyNode`:
  - Seeks the token stream to the recorded `token_start`
  - Parses the body using existing expression/statement/block parsing
  - Replaces the `UnbodyNode` with the real parsed body
- Performs semantic analysis alongside parsing (scope enter/exit, type checking)

**SOLVE** (`src/parser/solver.hpp/.cpp`)
- Post-processing on the fully parsed AST
- Resolves generics, expands macros, evaluates comptime expressions
- Stub for now — returns immediately

### Shared token helpers

Token-level operations (`lexeme`, `peek`, `advance`, `match`, `expect`, `consume`) are
extracted as free functions in `src/parser/token-helpers.hpp`. Both Scanner and Parser
use these, avoiding code duplication.

### UnbodyNode

A new AST expression node (`src/ast/ast-nodes.hpp`):

```cpp
struct UnbodyNode {
    memory::Span body_span;   // byte span in source file
    uint32_t token_start;     // token index for cursor seek
    uint32_t token_end;       // token index after the closing brace
};
```

### Symbol table

`SymbolTable::declare()` and `SymbolTable::lookup()` are implemented (currently stubs).
A flat `DynArray<SymbolData>` stores each symbol's name and declaring scope.

### Pipeline wiring

In `CompilationSession::parseStage()`:

```cpp
parser::Scanner scanner(tokens_, ast_builder_, diags_, syms_);
scanner.scanTopLevel(program_);

parser::Parser parser(tokens_, ast_builder_, diags_);
parser.parseProgram(program_);

parser::Solver solver(syms_, types_, diags_);
solver.solve(program_);
```

No separate `Session` struct — `CompilationSession` already owns all shared state and
passes individual references to each stage, consistent with the existing `import::Resolver`
pattern.

## Consequences

- **Forward references work**: All symbols are registered before any body is parsed,
  enabling functions to call each other in any order.
- **Generics/macros/comptime have a natural home**: SOLVE handles them without
  complicating the parser or semantic analyzer.
- **Smaller files**: Each stage has a single responsibility and is much shorter than
  the current monolithic parser.
- **`import::Resolver` becomes redundant**: SCAN handles top-level symbol registration;
  the Resolver class can be removed or merged into SEMA.
- **Body parsing is slightly slower** (two passes over tokens), but SCAN is extremely
  fast (no expression parsing, just brace counting and name registration).

## Files

| File | Action | Purpose |
|------|--------|---------|
| `src/parser/token-helpers.hpp` | NEW | Shared free functions for token ops |
| `src/parser/scanner.hpp/.cpp` | NEW | SCAN phase |
| `src/parser/parser.hpp/.cpp` | REFACTOR | SEMA phase (was monolithic parser) |
| `src/parser/solver.hpp/.cpp` | NEW | SOLVE phase (stub) |
| `src/parser/parse-result.hpp` | KEEP | May remove Result wrapper later |
| `src/parser/recovery.hpp/.cpp` | KEEP | Error recovery for body parsing |
| `src/ast/ast-nodes.hpp` | MODIFY | Add `UnbodyNode` |
| `src/ast/ast-builder.hpp/.cpp` | MODIFY | Add `unbody()` factory |
| `src/import/symbol-table.hpp/.cpp` | MODIFY | Implement `declare()`/`lookup()` |
| `src/cli/compilation-session.cpp` | MODIFY | Wire SCAN->Parser->Solver |
