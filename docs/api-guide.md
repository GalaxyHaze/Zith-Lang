# Zith Compiler API Guide

## Pipeline Overview

The compiler processes source files through 9 sequential stages:

```
Source -> Lexed -> Parsed -> Resolved -> TypeChecked -> Solved -> HirLowered -> MirLowered -> ZirInterpreted
```

Each stage maps to a method on `CompilationSession` and is gated by `PipelinePlan`.

## Key APIs by Subsystem

### 1. CLI (`src/cli/`)

- **`CompilationSession`** — Per-file compiler state. Owns arenas, AST builder, symbol table, types, and intermediate representations. Thread-safe by design (no shared mutable state).
  - `runTo(Stage)` — Runs the pipeline up to a given stage
  - `emitDiagnostics()` — Runs heuristics on accumulated diagnostics, then emits them
  - `hasErrors()` — Check if any errors were encountered

- **`Options`** — CLI flags parsed from `zithc [command] [args]`. Includes mode, optimization level, include directories, emit targets.

- **Commands** (`cmd/`): `build`, `compile`, `run`, `check`, `deps`, `fmt`, `repl`.

### 2. Lexer (`src/lexer/`)

- **`Lexer`** — Character-level scanner. Produces a flat `TokenStream` array.
  - Comment styles: `//` (line), `///` (doc), `/* */` (block), `/** */` (doc block)
  - String literals: `"..."` and `'...'`
  - Number literals: decimal, hex (`0x`), octal (`0o`), binary (`0b`), float
  - Identifiers: alphanumeric + `_`, starting with alpha or `_`

- **`Token`** — `{ kind, punc, span }`. TokenKind includes: `Operators`, `Punctuation`, `Identifier`, `LitVal`, `Fn`, `Struct`, `Trait`, `Interface`, `Implement`, `Control`, `If`, `Else`, `For`, `While`, `Variable`, `Visibility`, `Module`, `Logical`, `Typedef`, `Comments`, `Docs`, `End`.

- **`TokenStream`** — Bounded cursor over token array. Supports `peek(n)`, `advance()`, `match(kind)`.

- **`keyword_table.hpp`** — Perfect-hash keyword-to-TokenKind mapping. Keywords in shared TokenKind groups:
  - `TokenKind::Struct`: `struct`, `enum`, `union`, `component`
  - `TokenKind::Trait`: `trait`, `capability`, `extends`, `dyn`
  - `TokenKind::Interface`: `interface`

### 3. Parser (`src/parser/`)

Two-pass design:

#### Pass 1: Scan (`Parser::scan()`)
Registers top-level declarations (fn, struct, enum, union, component, trait, interface) in the symbol table. Function bodies are stored as `UnbodyNode` (raw token ranges) and skipped during scan.

#### Pass 2: Expand bodies (`Parser::expandBodies()`)
Re-parses previously-skipped function bodies. Tokens are re-read from the stored offset range.

#### Expression parsing (`parseExpr`/`parsePrimary`)
Recursive-descent + Pratt parser with operator precedence:
- Lowest: `or`, `xor`, `and` (word operators)
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Shift: `<<`, `>>`
- Additive: `+`, `-`
- Multiplicative: `*`, `/`, `%`
- Unary prefix: `-`, `!`, `not`
- Primary: literals, identifiers, `()`, `{}`, `if`, `while`, `for`

#### Key types:
- **`ScanResult`** — Holds `ScanEntry` arrays for fns, structs, enums, unions, components, traits
- **`ScanEntry`** — `{ name, span, body_node }` where body_node is an `UnbodyNode` reference

### 4. AST (`src/ast/`)

Arena-backed, flat ID-based AST:

- **`AstBuilder`** — Creates and owns all AST nodes. Returns `ExprId`, `StmtId`, `DeclId` (uint32_t indices).
  - Expression builders: `litExpr`, `ident`, `binary`, `unary`, `call`, `field`, `index`, `block`, `ifExpr`, `whileExpr`
  - Statement builders: `letStmt`, `assign`, `retStmt`
  - Declaration builders: `fnDecl`, `structDecl`, `enumDecl`, `unionDecl`, `componentDecl`, `traitDecl`, `interfaceDecl`, `importDecl`

- **Node types** (in `ast-nodes.hpp`):
  - `ExprNode` variant: `LitValue`, `IdentNode`, `BinaryNode`, `UnaryNode`, `CallNode`, `BlockNode`, `IfNode`, `WhileNode`, `FieldNode`, `IndexNode`, `UnbodyNode`
  - `StmtNode` variant: `LetNode`, `AssignNode`, `RetNode`, `ExprId`
  - `DeclNode` variant: `FnDeclNode`, `StructDeclNode`, `EnumDeclNode`, `UnionDeclNode`, `ComponentDeclNode`, `TraitDeclNode`, `InterfaceDeclNode`, `ImportNode`

- **`StructField`** — `{ name, type_expr }`
- **`EnumVariant`** — `{ name, fields, discriminant }`
- **`UnionVariant`** — `{ name, type_expr }`
- **`TraitMethod`** — `{ name, params }`

### 5. Diagnostics (`src/diagnostics/`)

- **`DiagnosticEngine`** — Accumulates `Diagnostic` objects. Methods: `report()`, `emit()`, `hasErrors()`.
  - `emit()` — Prints all diagnostics to stderr with source snippets, error codes, labels, suggestions, and tips
  - `diagnostics()` — Returns mutable reference to the internal diagnostics array

- **`Diagnostic`** — `{ severity, code, message, primary_span, labels, suggestions }`
  - `suggestions` — `std::vector<std::string>` of actionable hints shown as `= help: ...`

- **`Severity`** — `Note`, `Warning`, `Error`, `Bug`

- **Error codes** (`error-codes.hpp`): `E1001`-`E5002` for compile errors, `E10001`-`E10004` for runtime.
  - Each code has a registered `tip` string via `lookupError()`.

- **`HeuristicEngine`** — Generates "did you mean?" suggestions for `UndefinedIdent` errors using Levenshtein distance against all known symbol names. Also provides type mismatch hints.

### 6. Symbol Table (`src/import/`)

- **`SymbolTable`** — Flat scope-based name registry.
  - `declare(name, vis, depth, kind)` — Register a new symbol
  - `lookup(name)` — Find a symbol by name (walks parent scopes)
  - `get(id)` — Access symbol data by ID
  - `symbolCount()` — Total registered symbols
  - `enterScope()` / `exitScope()` — Scope management

- **`SymbolData`** — `{ name, scope, visibility, mod_depth, kind, decl_id, span, members }`
- **`SymKind`** — `Fn`, `Struct`, `Trait`, `Interface`, `Enum`, `Alias`, `Variable`, `Module`, `Component`, `Union`

- **`ImportManager`** — Resolves `import`/`from` declarations by searching visible roots and stdlib.

### 7. Types (`src/types/`)

- **`TypeIntern`** — Interned type storage. Creates canonical type IDs.
- **`Unifier`** — Type unification and checking. Handles int/float/bool/string/char, pointers, arrays, type variables.

### 8. Semantic Analysis (`src/sema/`)

- **`SemaPipeline`** — Type-checks and lowers AST to HIR.
  - `run(program)` — Two-pass: first register all fn stubs, then visit each fn body
  - Produces `HirModule` with typed, validated IR

### 8a. Solver (`src/solve/`)

- **`Solver`** — Generic constraint checking and monomorphization pass. Runs between type-checking and HIR lowering.
  - `collectGenerics()` — Finds generic type parameters in function signatures
  - `verifyGenericConstraints()` — Checks numeric bounds (`T: i32`, `T: f32`, etc.)
  - `resolveIncompleteInFn()` — Replaces unresolved type variables with concrete types
  - Constructor takes `TypeIntern&, AstBuilder&, ProgramNode&, SymbolTable&, DiagnosticEngine&, Arena&`

Currently handles only numeric constraint bounds; generics infrastructure ready for future expansion.

### 9. HIR/MIR/ZIR (`src/zir/`)

- **`HirModule`** / **`HirFunction`** — Typed high-level IR. Functions have entry blocks with `HirExpr` instructions.
  - HirExpr variants: `HirLiteral`, `HirVar`, `HirBinary`, `HirUnary`, `HirCall`, `HirLet`, `HirRet`, `HirBranch`, `HirJump`, `HirPhi`

- **`MirModule`** / **`MirFunction`** — Mid-level IR with basic blocks and typed opcodes. Lowered from HIR via `lowerFromHir()`.

- **`ZirModule`** / **`ZirFn`** / **`ZirBlock`** / **`ZirInst`** (`lib/zasm/`) — Register-based 64-bit fixed-encoding IR. Compiled from `.zas` assembly via `Assembler`, interpreted by `ThreadedInterp` or lowered to LLVM IR.
  - 76 opcodes: `Halt`, `Nop`, `Unreachable`, `Mov`/`Li`/`Ld`/`St`/`Ldr`/`Str`/`Lea`, `Add`--`Ashr` (ALU), `Seqz`--`Sgeu` (int compare), `FSeq`--`FSge` (float compare), `Sext`--`FpExt` (conversions), `Br`/`Bz`/`Bnz`/`Bgt`/`Bge`/`Blt`/`Ble` (branches), `Call`/`CallExt`/`Ret`, `Select`, `Alloca`/`Memset`/`Memcpy`, `Syscall`, `TlsLd`/`TlsSt`
  - See `lib/zasm/docs/zir-asm.md` for instruction encoding API and opcode field reference.

### 10. Memory (`src/memory/`)

- **`Arena`** — Bump-allocator for tree structures. No per-allocation free.
- **`DynArray<T>`** — Arena-backed dynamic array with pointer stability and move semantics.
- **`SourceMap`** — File registry with source text access and span resolution.
- **`Span`** — `{ file_id, start, end }` byte offset range.
- **`Result<T>`** — Error-propagating result type.

## Compiler Invocation

```bash
# Compile a single file (to MIR)
zithc compile file.zith

# With verbose output and AST dump
zithc compile -v --emit-ast file.zith

# With HIR dump
zithc compile --emit-hir file.zith

# Build a project
zithc build

# Run with bytecode interpreter
zithc run file.zith --interpreted
```

## Adding a New Feature

1. **Lexer**: Add token kind in `token.hpp`, map keyword in `keyword-table.hpp`
2. **Parser**: Add scanning in `scan()` (first pass), parsing in `expandBodies()` or expression/statement parsers
3. **AST**: Add node type in `ast-nodes.hpp`, builder method in `ast-builder.hpp/cpp`, update `DeclNode`/`ExprNode`/`StmtNode` variants
4. **Printer**: Add visitor in `ast-printer.cpp`
5. **Sema**: Add lowering in `sema-pipeline.cpp`
6. **HIR**: Add instruction type if needed in `hir-expr.hpp`
7. **Solver**: Add constraint checking in `solver.cpp` if new generic bounds are needed
8. **ZIR**: Add opcode in `zir-inst.hpp`, handler in `zir-interp.hpp`, lowering in `zir-emitter.cpp`
9. **Tests**: Add unit test in `tests/unit/`

## Key Design Decisions

- **Arena allocation**: All AST, symbol table, and IR nodes are arena-allocated for performance and simplicity.
- **Flat scope in scanner**: All names (fn params, struct fields) share a single flat scope during scanning. This means names must be globally unique within a file.
- **Two-pass parsing**: Scan phase registers symbols and skips fn bodies. Body expansion phase re-parses token ranges. This enables forward references.
- **Per-file sessions**: Each file gets its own `CompilationSession` with independent state, enabling future parallel compilation.
- **Diagnostic heuristics**: Suggestions are generated by `HeuristicEngine` before emission, using Levenshtein distance for "did you mean?" suggestions.
