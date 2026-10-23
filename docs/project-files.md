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
| `src/ir/hir.zith` | Frontend-neutral HIR nodes, nominal type definitions, functions, and residual attributes. | Shared type and definition tables aligned with the cross-dialect cache contract. |
| `src/ir/hir-verify.zith` | Verifies HIR references, definition tables, node shapes, and residual safety metadata. | Structural verifier with CFG-dominating guard validation. |
| `src/cache/cache.zith` | Shared HIR identity and derived-VIR compatibility rules. | Identity checks and version gates; ZIRL framing is implemented in `zirl-artifact.zith`. |
| `src/cache/zirl-buffer.zith` | Bounds-checked little-endian ZIRL byte primitives. | Signed/unsigned integers, `f64`, and atomic length-prefixed blobs. |
| `src/cache/zirl-container.zith` | ZIRL v21 header, section-table records, and compatibility checks. | Validates required HIR sections and safely skippable optional sections. |
| `src/cache/zirl-artifact.zith` | Complete ZIRL container envelope reader and writer. | Persists identity, dependencies, section table, and payloads with checksum validation; section payloads are supplied by the section codecs. |
| `src/cache/zirl-type-section.zith` | ZIRL metadata type-entry codec. | Encodes and transactionally decodes compact type records for the Metadata section. |
| `src/cache/zirl-metadata.zith` | ZIRL Metadata section codec. | Encodes and decodes schema-2 module metadata with caller-owned storage. |
| `src/cache/zirl-decl-section.zith` | ZIRL Decls section codec. | Encodes and decodes declaration records and their field/method references with caller-owned storage. |
| `src/cache/zirl-template-section.zith` | ZIRL Templates section codec. | Encodes and decodes generic parameter bounds and blueprint type/name/reference tables with caller-owned storage. |
| `src/cache/zirl-attrs-section.zith` | ZIRL Attrs section codec. | Encodes residual slot, call, function, and safety-proof facts with their original HIR indices and caller-owned decode storage. |
| `src/cache/zirl-code-section.zith` | ZIRL Code section codec. | Encodes compact expressions, functions/blocks, globals, vtables, and canonical mappings with caller-owned decode storage. |
| `src/cache/zirl-canonical-types.zith` | Artifact-owned canonical HIR type table and synthesized function signatures. | Remaps HIR-local type IDs and returns structured validation/capacity errors. |
| `src/cache/zirl-hir-code.zith` | HIR-to-ZIRL Code section projection. | Verifies HIR, remaps type/function identities, and builds compact code records using caller-owned storage. |
| `src/cache/zirl-hir-artifact.zith` | Composition of required typed HIR sections into a ZIRL artifact. | Encodes Metadata, Decls, Templates, Code, and Attrs with fixed section IDs/schema versions before writing the artifact envelope. |

There is no project-owned C source file in the current tree. Add its entry
here if one is introduced; do not infer that a planned directory already
exists.

### Tests

| Path | Role | Status |
|---|---|---|
| `tests/README.md` | Documents the type-check and behavioral test commands. | Test convention. |
| `tests/run.sh` | Runs type-check and behavioral tests in an isolated temporary project. | Test harness. |
| `tests/run.zith` | VM behavioral test entry point. | Smoke fixture. |
| `tests/hir-contract.zith` | Native test entry point for the shared HIR data contract. | Exercises HIR invariants, ZIRL section round-trips, HIR-to-Code projection, golden bytes, malformed records, and buffer failures. |
| `tests/canonical-types.zith` | Native test entry point for artifact-owned canonical types. | Exercises local type IDs, synthesized signature metadata, insufficient storage, and invalid function references. |
| `tests/zirl-artifact.zith` | Native test entry point for complete ZIRL container framing. | Exercises identity/dependency round-trip, required section layout, checksum rejection, malformed sections, and short output buffers. |
| `tests/zirl-hir-artifact.zith` | Native test entry point for typed HIR-artifact composition. | Calls the typed section writer with an empty HIR fixture; success-path layout and buffer-failure assertions remain incomplete. |

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
