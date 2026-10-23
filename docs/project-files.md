# Project File Map

This document describes the role of the files and directories in this
repository. It is an inventory and navigation aid, not a build manifest.

`ZithProject.toml` is the project configuration consumed by the Zith toolchain.
At present it declares project metadata, `src/main.zith` as the build entry
point, and directory paths; it does not enumerate source files individually.

## Current Files

### Root

| Path | Role |
|---|---|
| `AGENTS.md` | Instructions for coding agents working in this repository. |
| `CONTEXT.md` | Canonical project vocabulary. |
| `ZithProject.toml` | Project metadata, build entry point, and standard paths. |
| `.gitignore` | Files excluded from version control. |

### Source

| Path | Role | Status |
|---|---|---|
| `src/main.zith` | Current executable entry point. | Bootstrap source; imports `utils/prints`. |
| `src/utils/prints.zith` | Colored terminal output helpers. | Bootstrap source using C stdio interop. |
| `src/frontend/lexer.zith` | Reserved frontend lexer source. | Experimental implementation. |
| `src/ir/hir.zith` | Frontend-neutral HIR nodes, types, functions, and residual attributes. | Initial shared contract; verifier and cache codec follow. |

There is no project-owned C source file in the current tree. Add its entry
here if one is introduced; do not infer that a planned directory already
exists.

### Tests

| Path | Role | Status |
|---|---|---|
| `tests/README.md` | Documents the type-check and behavioral test commands. | Test convention. |
| `tests/run.sh` | Runs type-check and behavioral tests in an isolated temporary project. | Test harness. |
| `tests/run.zith` | VM behavioral test entry point. | Smoke fixture. |
| `tests/hir-contract.zith` | Native test entry point for the shared HIR data contract. | Initial variant-construction fixture. |

### Documentation

| Path | Role |
|---|---|
| `docs/architecture.md` | Compiler modules, seams, stages, and proposed source layout. |
| `docs/adr/` | Accepted local architecture decisions. |
| `docs/agents/` | Agent workflow, domain-documentation, and issue-tracker guidance. |
| `docs/notes/` | Proposals and working notes that are not architecture decisions. |
| `docs/plans/` | Roadmaps and execution plans, including bootstrap and cross-compilation. |
| `docs/reference/full-zith/` | Imported full-Zith design material, kept as a reference baseline. |

## Intended Source Layout

The following is the target module layout from
[`architecture.md`](architecture.md), not a claim that all these files or
directories exist today:

```text
src/
  context/                 arenas, interner, diagnostics
  frontend/                Lex, Scan, Import, Resolve
  sema/                    semantic analysis
  proof/                   ZPK: NIA, RRA, MRA, NRA
  ir/                      HIR
  backends/
    vir/                   HIR to VIR
    vm/                    VIR execution
    tiny/                  future VIR-to-C-object backend
    llvm/                  HIR to LLVM IR
  drivers/
    cli/                   command-line driver
    lsp/                   language-server driver
    wasm/                  WASM playground driver
  cache/                   compiler artifact and fact cache
  cinterop/                C binder and C ABI surface
  os/                      operating-system boundary
  utils/                   shared utilities, only where module ownership is
                           not more appropriate
```

Use one source file per cohesive responsibility where practical; a directory
represents a module area, not a requirement to create placeholder files for
every planned component. Keep this map synchronized when source files or
top-level documentation areas are added, moved, or removed. For architecture
decisions about the source tree, see
[`ADR-0011`](adr/0011-source-tree-by-area.md); for implementation and migration
order, see the [roadmap](plans/roadmap.md).
