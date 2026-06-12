# Architecture Plan — Force & Save

**Goal:** Finalize the rewrite architecture and execute to completion.

## Phase A — Force the Architecture (Decisions to Lock)

### A1. Confirm module layout
```
src/              # All compiler source (no public headers)
  cli/            # CLI entry, options, compilation session, commands
  lexer/          # Tokenizer, keyword table, token types
  parser/         # Recursive-descent + Pratt parser
  ast/            # Arena-backed AST builder and node types
  types/          # TypeIntern, Unifier, TypeKind definitions
  sema/           # Semantic analysis pipeline (type-check, resolve, lower)
  import/         # Symbol table, import resolution, module system
  diagnostics/    # Diagnostic engine, emitter, error codes
  memory/         # Arena, dyn-array, optional, result, string-interner, source-map
  zir/            # Intermediate representation
    hir/          # High-level IR (typed AST)
    mir/          # Mid-level IR (opcodes, basic blocks)
stdlib/           # Standard library (.zith files)
tests/unit/       # Unit tests (one per subsystem)
```

**Decision:** Lock this layout. No more structural changes.

### A2. Confirm pipeline stages
```
Source → Lexed → Parsed → Resolved → TypeChecked → HirLowered → MirLowered → ZirInterpreted
```

**Decision:** These 8 stages are final. Each maps to a `CompilationSession` method.

### A3. Confirm technology choices
| Choice | Decision | Rationale |
|--------|----------|-----------|
| Language | C++20 | Broad compiler support (GCC 11+, Clang 14+, MSVC 2022+) |
| Build | CMake 3.20+ | Cross-platform, FetchContent for deps |
| Dependencies | mio, tomlplusplus | Minimal, header-only or nearly so |
| IR design | HIR → MIR two-tier | Decouples semantic analysis from codegen |
| Allocation | arena-only | No global thread_local, deterministic cleanup |
| Error handling | diagnostics::DiagnosticEngine | Centralized, no exceptions in compiler core |

**Decision:** Lock these. No new dependencies without strong justification.

## Phase B — Save the Plan (Execution Roadmap)

### B1. Immediate (this week)
- [ ] **Fix clang-format** — run `cmake --build build -t fmt`
- [ ] **Implement parser** — replace all `kInvalid*` stubs with real parsing
- [ ] **Wire parser into pipeline** — `CompilationSession::parseStage()` returns real AST

### B2. Short-term (next 2 weeks)
- [ ] **Implement TypeIntern** — real type deduplication, structural comparison
- [ ] **Implement Unifier** — type unification + error reporting
- [ ] **Wire sema pipeline** — `SemaPipeline::run()` traverses AST → produces typed HIR

### B3. Medium-term (next month)
- [ ] **Fix HIR `addFn()`** — stop dropping function names
- [ ] **Implement HIR verifier** — validate expression types, CFG structure
- [ ] **Implement MIR lowering** — `MirLowering::lower()` translates HIR → MIR
- [ ] **Implement MIR verifier** — validate operands, basic block links

### B4. Long-term (next quarter)
- [ ] **ZIR interpreter** — portable bytecode runner
- [ ] **LLVM backend** — native code generation
- [ ] **Source-aware diagnostics** — snippets with underlines
- [ ] **Test coverage** — property-based + fuzz testing

## Phase C — Quality Gates (Save Points)

Each gate must pass before proceeding:

| Gate | Check |
|------|-------|
| C1 | All tests pass (`ctest --test-dir build`) |
| C2 | Clean compile with no warnings (`-Wall -Wextra -Wpedantic`) |
| C3 | Format check passes (`cmake --build build -t fmt-check`) |
| C4 | No clang-tidy violations (if configured) |
| C5 | Each new feature has ≥1 unit test |

## Phase D — Commit Strategy

| Commit type | Message prefix | Example |
|-------------|----------------|---------|
| Architecture doc | `docs/arch: ` | `docs/arch: add architecture comparison and plan` |
| Parser work | `feat/parser: ` | `feat/parser: implement binary expression parsing` |
| Type system | `feat/types: ` | `feat/types: implement TypeIntern deduplication` |
| Bug fix | `fix: ` | `fix: HIR addFn() drops function name` |
| Tests | `test: ` | `test: add parser expression round-trip tests` |
| Cleanup | `chore: ` | `chore: run clang-format on all sources` |

## Sign-off

This plan supersedes all previous architectural documents. All new work must follow this plan. Deviations require a documented exception in ARCH-PLAN.md.
