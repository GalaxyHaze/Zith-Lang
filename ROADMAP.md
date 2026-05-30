# Roadmap — Zith Rewrite

## Legend

- ✅ Done — fully implemented
- ◐ Stub — declarations created, no logic
- ⬜ Pending — not started

---

## Phase 0 — Infrastructure

The foundation layer used by all other modules.

| # | File | Description | Status |
|---|------|-------------|--------|
| 1 | `src/infra/util/result.hpp` | `Result<T,E>` monadic error handling type | ✅ Done |
| 2 | `src/infra/util/optional.hpp` | Lightweight optional value container | ✅ Done |
| 3 | `src/infra/alloc/arena.hpp` | Bump allocator with block chains and rollback scopes | ✅ Done |
| 4 | `src/infra/collections/dyn-array.hpp` | Arena-backed dynamic array with in-place extension | ✅ Done |
| 5 | `src/infra/interner/string-interner.hpp` | String dedup table for identifier names | ✅ Done |

---

## Phase 1 — Frontend: Source Layer

File and position tracking for error reporting and lexer input.

| # | File | Description | Status |
|---|------|-------------|--------|
| 6 | `src/frontend/source/span.hpp` | `ByteOffset`, `Loc` (line/col), `Span` (file+range), `FileId` | ✅ Done |
| 7 | `src/frontend/source/source-file.hpp` | `SourceLoc` — mmap-backed file representation with line index | ✅ Done |
| 8 | `src/frontend/source/source-map.hpp` | Thread-safe file registry with double-checked locking | ✅ Done |

---

## Phase 2 — Frontend: Lexer

Tokenizes source text into a stream of tokens for the parser.

| # | File | Description | Status |
|---|------|-------------|--------|
| 9 | `src/frontend/lexer/token.hpp` | `TokenKind` enum, `Token` struct, `TokenStream` with peek/advance/match | ✅ Done |
| 10 | `src/frontend/lexer/keyword-table.hpp` | Compile-time perfect hash table mapping keywords to `TokenKind` | ✅ Done |
| 11 | `src/frontend/lexer/lexer.hpp` | `Lexer` class declaration + `tokenize()` free function | ✅ Done |
| 12 | `src/frontend/lexer/lexer.cpp` | Full lexer: numbers, strings, identifiers, comments, punctuation | ✅ Done |
| 13 | `tests/unit/lexer-tokens.cpp` | Lexer unit tests (keyword mapping, literals, edge cases) | ✅ Done |

---

## Phase 3 — Diagnostics

Error/warning model and output for the whole compiler pipeline.

| # | File | Description | Status |
|---|------|-------------|--------|
| 14 | `src/diagnostics/model/diagnostic.hpp` | `Severity`, `Label`, `Diagnostic` structs | ✅ Done |
| 15 | `src/diagnostics/model/labels.hpp` | `LabeledSpan`, `Note` helpers | ✅ Done |
| 16 | `src/diagnostics/engine/diagnostic-engine.hpp` | `DiagnosticEngine` — collects and queries diagnostics | ✅ Done |
| 17 | `src/diagnostics/engine/emitter.hpp` | `Emitter` — formats diagnostics to stderr with source snippets | ✅ Done |

---

## Phase 4 — Frontend: AST

Arena-allocated abstract syntax tree nodes produced by the parser.

| # | File | Description | Status |
|---|------|-------------|--------|
| 18 | `src/frontend/ast/ast-ids.hpp` | `NodeId`, `ExprId`, `StmtId`, `DeclId` index types | ✅ Done |
| 19 | `src/frontend/ast/ast-nodes.hpp` | Variant-based AST node types (Literal, Binary, If, FnDecl, etc.) | ✅ Done |
| 20 | `src/frontend/ast/ast-builder.hpp` | `AstBuilder` — factory for arena-allocated AST nodes | ✅ Done |

---

## Phase 5 — Frontend: Parser

Recursive-descent parser that consumes tokens and builds AST nodes.

| # | File | Description | Status |
|---|------|-------------|--------|
| 21 | `src/frontend/parser/parse-result.hpp` | `ParseResult<T>` — parser output with diagnostics | ✅ Done |
| 22 | `src/frontend/parser/parser.hpp` | `Parser` class with precedence climbing expression parsing | ◐ Stub |
| 23 | `src/frontend/parser/parser.cpp` | Parser implementation: expressions, statements, declarations | ◐ Stub |
| 24 | `src/frontend/parser/recovery.hpp` | Error recovery: panic-sync to safe tokens | ◐ Stub |
| 25 | `tests/unit/parser-expr.cpp` | Expression parsing tests (literals, binary ops, calls, precedence) | ◐ Stub |
| 26 | `tests/unit/parser-stmt.cpp` | Statement parsing tests (let, if, for, match, return) | ◐ Stub |

### Parser — Implementation Checklist

- [ ] **Expression parsing** — Precedence climbing for binary ops, unary, calls, field/index access, parenthesized
- [ ] **Statement parsing** — `let`/`var`/`const`, assignment, `if`/`else`, `for`/`in`, `match`, `return`, `break`, `continue`, expression statements
- [ ] **Declaration parsing** — `fn` (params, return type, body), `struct` (fields), `import` (module paths), `mod`
- [ ] **Error recovery** — On unexpected token, skip to semicolon/brace/EOF and keep parsing

---

## Phase 6 — Middleend: Symbols

Name binding and scope management.

| # | File | Description | Status |
|---|------|-------------|--------|
| 27 | `src/middleend/symbols/symbol-id.hpp` | `SymId`, `ScopeId` index types | ✅ Done |
| 28 | `src/middleend/symbols/symbol-table.hpp` | `SymbolTable` — hierarchical scopes with name → SymId mapping | ◐ Stub |
| 29 | `src/middleend/symbols/resolver.hpp` | `Resolver` — walks AST, populates symbol table, resolves references | ◐ Stub |

### Symbols — Implementation Checklist

- [ ] **Scope management** — `enterScope`/`exitScope`, parent chain, root scope
- [ ] **Declaration** — Register symbols in current scope, detect duplicates
- [ ] **Lookup** — Walk scope chain, resolve `IdentExpr` to `SymId`, report unresolved

---

## Phase 7 — Middleend: Types

Type representation, interning, and unification.

| # | File | Description | Status |
|---|------|-------------|--------|
| 30 | `src/middleend/types/type-id.hpp` | `TypeId` index with built-in constants (kVoid, kInt, etc.) | ✅ Done |
| 31 | `src/middleend/types/type-kind.hpp` | `TypeKind` enum + variant-based `TypeData` (Int, Float, Ptr, Fn, etc.) | ✅ Done |
| 32 | `src/middleend/types/type-intern.hpp` | `TypeIntern` — hash-consing dedup table for types | ◐ Stub |
| 33 | `src/middleend/types/unify.hpp` | `Unifier` — constraint solver with type variables and substitution | ◐ Stub |

### Types — Implementation Checklist

- [ ] **Type interning** — Deduplicate structurally identical types, lookup by ID
- [ ] **Type variables** — Fresh variable generation for inference
- [ ] **Unification** — Unify two types, occurs check, substitution map
- [ ] **Built-in types** — Pre-populate void, bool, char, int/float of various widths

---

## Phase 8 — Middleend: Semantic Analysis + HIR

Type-checked, lowered IR with resolved names and concrete types.

| # | File | Description | Status |
|---|------|-------------|--------|
| 34 | `src/middleend/sema/sema-context.hpp` | `SemaContext` — bundles symbols, types, diagnostics, builder | ◐ Stub |
| 35 | `src/middleend/sema/sema-result.hpp` | `SemaResult` — HIR module + diagnostics + symbol table | ✅ Done |
| 36 | `src/middleend/sema/sema-pipeline.hpp` | `SemaPipeline` — orchestrates resolution, type-checking, lowering | ◐ Stub |
| 37 | `src/middleend/hir/hir-types.hpp` | HIR-level types (post-resolved, no type variables) | ✅ Done |
| 38 | `src/middleend/hir/hir-expr.hpp` | Flat HIR expression representation (index-based, no recursion) | ✅ Done |
| 39 | `src/middleend/hir/hir-module.hpp` | `HirModule` — function containers with basic blocks | ◐ Stub |
| 40 | `src/middleend/hir/hir-verify.hpp` | `HirVerifier` — validates HIR invariants | ◐ Stub |
| 41 | `tests/unit/sema-basics.cpp` | Sema tests (type checking, symbol resolution, inference) | ◐ Stub |

### Sema + HIR — Implementation Checklist

- [ ] **Name resolution** — Walk AST, call Resolver, populate symbol table
- [ ] **Type checking** — Walk typed AST, infer types, call Unifier, report mismatches
- [ ] **HIR lowering** — Transform typed AST into flat HIR representation
- [ ] **HIR verification** — Check well-typedness, valid references, no dangling IDs

---

## Phase 9 — Middleend: MIR

Lowered 3-address-code IR suitable for code generation.

| # | File | Description | Status |
|---|------|-------------|--------|
| 42 | `src/middleend/mir/mir-inst.hpp` | `MirOpcode`, `MirInst`, `ValueId`, `MirBasicBlock` | ✅ Done |
| 43 | `src/middleend/mir/mir-module.hpp` | `MirModule` — functions with basic blocks and MIR instructions | ◐ Stub |
| 44 | `src/middleend/mir/mir-lower-from-hir.hpp` | `MirLowering` — HIR function → MIR basic blocks translation | ◐ Stub |
| 45 | `src/middleend/mir/mir-verify.hpp` | `MirVerifier` — SSA form, valid ops, well-terminated blocks | ◐ Stub |
| 46 | `tests/unit/mir-lowering.cpp` | MIR lowering tests (control flow, calls, array access) | ◐ Stub |

### MIR — Implementation Checklist

- [ ] **Basic block formation** — Split HIR control flow into linear basic blocks
- [ ] **Instruction selection** — Map HIR ops to MIR opcodes
- [ ] **SSA construction** — Phi nodes at merge points
- [ ] **Verification** — Validate SSA dominance, type consistency, no dangling values

---

## Phase 10 — Backend Interface

Abstract backend contract for code generation.

| # | File | Description | Status |
|---|------|-------------|--------|
| 47 | `src/backend/interface/backend.hpp` | Abstract `Backend` base class with `compile(MirModule) → BackendResult` | ✅ Done |
| 48 | `src/backend/interface/backend-options.hpp` | `OptLevel`, `OutputFormat` flags | ✅ Done |
| 49 | `src/backend/interface/backend-result.hpp` | `BackendResult` — bytecode buffer + diagnostics | ✅ Done |
| 50 | `src/backend/interface/target-info.hpp` | `TargetInfo` — arch, OS, pointer size, endianness | ✅ Done |

---

## Phase 11 — Backend: VM

Bytecode format and emitter for the built-in virtual machine.

| # | File | Description | Status |
|---|------|-------------|--------|
| 51 | `src/backend/vm/bytecode-format.hpp` | `BytecodeOp` enum, `BCInst` struct, `BytecodeHeader` | ✅ Done |
| 52 | `src/backend/vm/bytecode-emitter.hpp` | `BytecodeEmitter` — serializes MIR to binary bytecode | ◐ Stub |
| 53 | `src/backend/vm/vm-backend.hpp` | `VmBackend` — `Backend` subclass using the bytecode emitter | ◐ Stub |

### VM Backend — Implementation Checklist

- [ ] **Bytecode emission** — Walk MIR, emit bytecode instructions with fixups
- [ ] **Constant pool** — Dedup literals, emit pool table
- [ ] **VM runtime** — Bytecode interpreter/VM to execute emitted programs

---

## Phase 12 — Backend: LLVM (Stretch)

LLVM IR code generation for native compilation.

- [ ] **LLVM IR lowering** — Map MIR to LLVM IR instructions
- [ ] **Target selection** — x86_64, aarch64, wasm32 via LLVM backends

---

## Phase 13 — Driver

Pipeline orchestration and CLI.

| # | File | Description | Status |
|---|------|-------------|--------|
| 54 | `src/driver/options.hpp` | CLI options parsing (input files, emit flags, output) | ✅ Done |
| 55 | `src/driver/pipeline-plan.hpp` | Pipeline stage enum with `shouldStop`/`advance` | ✅ Done |
| 56 | `src/driver/compilation-session.hpp` | `CompilationSession` — per-invocation state and stage runners | ◐ Stub |
| 57 | `src/driver/compiler-driver.hpp` | `CompilerDriver` — top-level orchestrator | ◐ Stub |
| 58 | `src/main/zithc-main.cpp` | `main()` — parses args, calls driver | ✅ Done |

### Driver — Implementation Checklist

- [ ] **Stage pipeline** — Lex → Parse → Sema → MIR → Backend with error halting
- [ ] **Emit flags** — `--tokens`, `--emit-ast`, `--emit-hir`, `--emit-mir` for debugging
- [ ] **Error reporting** — Collect and display diagnostics from all stages

---

## Phase 14 — Tooling

- [ ] **LSP server** — Language Server Protocol implementation for IDE support
- [ ] **Package manager** — Dependency resolution and module fetching
- [ ] **Incremental compilation** — Reuse unchanged compilation units across builds
