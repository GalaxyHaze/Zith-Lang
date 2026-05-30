# Roadmap — Zith Rewrite

## Completed (Phase 1 — Infrastructure + Frontend)

| Layer | Status |
|-------|--------|
| `util::Result<T,E>` | Done |
| `util::Optional<T>` | Done |
| `alloc::Arena` + `MarkPoint` | Done |
| `collections::DynArray<T>` | Done |
| `SourceLoc` (mmap file I/O) | Done |
| `SourceMap` (thread-safe registry) | Done |
| Lexer (full tokenizer) | Done |
| `StringInterner` | Done |

## Stubs Created (declarations only, no logic)

| Layer | Files |
|-------|-------|
| Diagnostics | `diagnostic.hpp`, `labels.hpp`, `diagnostic-engine.hpp`, `emitter.hpp` |
| AST | `ast-ids.hpp`, `ast-nodes.hpp`, `ast-builder.hpp` |
| Parser | `parse-result.hpp`, `parser.hpp`, `recovery.hpp` |
| Symbols | `symbol-id.hpp`, `symbol-table.hpp`, `resolver.hpp` |
| Types | `type-id.hpp`, `type-kind.hpp`, `type-intern.hpp`, `unify.hpp` |
| Sema | `sema-context.hpp`, `sema-result.hpp`, `sema-pipeline.hpp` |
| HIR | `hir-types.hpp`, `hir-expr.hpp`, `hir-module.hpp`, `hir-verify.hpp` |
| MIR | `mir-inst.hpp`, `mir-module.hpp`, `mir-lower-from-hir.hpp`, `mir-verify.hpp` |
| Backend | Interface + VM bytecode format/emitter |
| Driver | `compiler-driver.hpp`, `options.hpp`, `pipeline-plan.hpp`, `compilation-session.hpp` |

## Phase 2 — Parser Implementation

- [ ] Expression parsing (precedence climbing)
- [ ] Statement parsing (let, if, for, match, return)
- [ ] Declaration parsing (fn, struct, import)
- [ ] Error recovery

## Phase 3 — Semantic Analysis

- [ ] Symbol resolution
- [ ] Type checking + unification
- [ ] HIR lowering

## Phase 4 — MIR

- [ ] HIR-to-MIR lowering
- [ ] Verification passes

## Phase 5 — Backend

- [ ] VM bytecode emission
- [ ] LLVM codegen (stretch)

## Phase 6 — Tooling

- [ ] LSP server
- [ ] Package manager
- [ ] Incremental compilation
