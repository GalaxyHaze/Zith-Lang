# Zith Compiler Architecture (Bootstrap Shape)

This is the proposed module and seam shape for the initial Zith compiler. The
compiler and project sources may initially use `Zith--` and C; those are
bootstrap substrates, not a commitment to keep the final implementation in
legacy syntax or C. The gradual migration is tracked in the
[roadmap](plans/roadmap.md). This document describes compiler architecture, not
the Zith language specification or stage internals.

Vocabulary is in [`CONTEXT.md`](../CONTEXT.md). The decisions behind this shape
are the initial compiler decisions in ADRs
[0001](adr/0001-compiler-core-exposes-one-narrow-seam.md) through
[0013](adr/0013-c-toolchain-is-an-adapter.md), read together with
[ADR-0014](adr/0014-full-zith-is-the-project-target.md) and
[ADR-0015](adr/0015-shared-hir-and-cross-dialect-cache.md). The cross-compile
strategy is in [the cross-compile plan](plans/cross-compile.md); the staged IR
work is in the [shared-HIR plan](plans/shared-hir-interoperability.md).

## Shape

```text
drivers/                      outside the core
  cli                         thin; one revision in, result out
  lsp (Workspace)             persistent context; one run per snapshot
  wasm                        compile once, execute from cache
        |
        v
core: compile(input) -> result
  context                     arenas + interner + diagnostic collector
        |
  frontend        Lex -> Scan -> Import -> Resolve
        |
  sema            Sema
        |
  proof (ZPK)     NIA -> RRA -> MRA -> NRA
        |
  ir              HIR
        |                    split at HIR
        +--> vir             HIR -> VIR
        |        |
        |        +--> vm     (executes VIR)
        |        +--> tiny   (future; consumes VIR)
        |
        +--> llvm            HIR -> LLVM IR (reused emitters)
        |
  cinterop                    C binder + C ABI surface
  os-c-layer                  OS-sensitive ops in C, behind one interface
        |
  cache                     transversal; canonical HIR, compatible VIR only
```

## Modules

**Context**: the shared, passed-by-reference state: arenas, the string/type
interner, and the diagnostic collector. It is not a session; it holds no
long-lived compilation state.

**Frontend**: Lex, Scan, Import, and Resolve as contracts. Turns source into the
resolved frontend representation.

**Sema**: semantic analysis over the resolved frontend representation.

**Proof (ZPK)**: the distinct proof phase. Sub-systems NIA, RRA, MRA, NRA run in
dependency order and emit residual facts into side tables.

**IR**: HIR, the shared last front-side representation and the single backend
entry point. Distinct frontends adapt their own semantic results to this
contract; frontend AST and session identities are not part of persisted HIR.

**Backends**: VIR and its consumers (VM now, tiny later), and the reused LLVM
emitters reading HIR directly.

**Drivers**: the CLI driver and the language-server driver. They own the
persistent state the core does not. A WASM playground driver is a third driver
over the same seam.

**C interop**: the current C binder and C ABI surface, a module behind the
external seam used by the frontend and backends. Its implementation is subject
to the C-to-Zith migration; the boundary must remain explicit during that work.
The C object emitter (the tiny backend) is a backend, not part of this module.

**OS-C layer**: the OS-sensitive operations (file system, process control,
time, memory mapping, terminal) isolated in C behind one narrow interface, so
the initial core stays target-agnostic. This is the current bootstrap boundary,
not a decision that project-owned OS code must remain in C permanently; review
it as full-Zith platform capabilities arrive. See
[ADR-0012](adr/0012-os-sensitive-code-is-a-c-layer.md) and
[ADR-0014](adr/0014-full-zith-is-the-project-target.md).

**C toolchain**: an adapter behind a seam with a `ToolchainBackend` contract
(native compile, link, header import, sysroot discovery). Adapters are tried in
a deterministic order: clang + `--target` + `lld` first, Zig second, native
cross-GCC third; see [ADR-0013](adr/0013-c-toolchain-is-an-adapter.md).

**Cache**: not a module in the call chain; a transversal concern materialized
once at the end by the driver. The cross-dialect cache contract is canonical
HIR, with VIR reusable only when its VM semantics match; backend outputs are
not shared. See [ADR-0015](adr/0015-shared-hir-and-cross-dialect-cache.md).

## Seams

**External seam**: `compile(input) -> result`. The only surface the core offers.
CLI, filesystem, process control, and linking are outside it.

**Stage seam**: `stage(immutable input, context) -> immutable output`. Every
stage presents this shape, so a test feeds a hand-built input across the same
seam a caller uses. Failure is an explicit `Result`; a stage's errors block the
following stages, warnings do not.

**Backend seam**: HIR. Every backend consumes HIR, directly or through VIR.

**External seam signature**: `compile(input) -> result`, where `input` is an
immutable revision (text, module identity, resolution root, immutable options)
and `result` carries structured diagnostics, artifacts (HIR/VIR/object), and
residual facts.

## Stage Contracts

Stage names are contracts, not modules. They are the shared vocabulary the ADRs
and notes cite; a module may own several of them.

The parser is mentioned as the consumer of Scan's token stream but is not
currently named as a stage or module. Resolve that contract before
implementation depends on it; the roadmap tracks it as a design-baseline task.

| Stage | Contract (one line) |
|---|---|
| Lex | Source text to tokens. |
| Scan | Tokens to the token stream the parser consumes. |
| Import | Module discovery and import requests. |
| Resolve | Names, symbols, and qualified access. |
| Sema | Type checking and the typed representation. |
| ZPK | Numeric, geometry, region, and ownership proof. |
| HIR | Lowering to the high-level IR. |
| VIR | Lowering HIR to the typed execution IR. |
| Backends | VM/tiny (VIR) and LLVM (HIR). |

## Drivers

| Driver | Input | Persistent state |
|---|---|---|
| CLI | one revision | none |
| LSP (`Workspace`) | one `Snapshot` per request | workspace context across snapshots |
| WASM playground | one revision, then cached replay | compile-once artifact cache |

Tests are consumers of the external seam, not a driver.

## Cross-Platform Builds

The initial compiler design keeps the core target-agnostic and confines
portability to the OS-C layer and C toolchain adapter (ADR-0012, ADR-0013).
Treat that boundary as bootstrap architecture and review it during the planned
C-to-Zith migration (ADR-0014).

The target matrix below is a build strategy, not a release promise. Validate
platform SDK terms and end-to-end execution before advertising a target.

| Target | How it is produced |
|---|---|
| Linux amd64/arm64 (glibc/musl) | clang + `--target` + `lld` with a sysroot, or Zig |
| WASM/WASI | `wasi-sdk` or Zig's wasm targets |
| Windows (GNU ABI) | Zig or llvm-mingw |
| Windows (MSVC ABI) | `xwin` + clang-cl + `lld-link` locally, or a Windows runner |
| macOS | Apple toolchain + SDK on a Mac; see the proposed [macOS build policy](notes/macos-build-policy.md) |

The macOS and Windows-MSVC paths depend on vendor SDKs and their applicable
terms. Treat them as local build strategies until reviewed for distribution;
the current macOS proposal is in
[macOS build policy](notes/macos-build-policy.md).

## Source Layout

```text
src/
  context/  frontend/  sema/  proof/  ir/
  backends/{vir,vm,tiny,llvm}/
  drivers/{cli,lsp,wasm}/
  cache/  cinterop/  os/
```

One folder per module; see
[ADR-0011](adr/0011-source-tree-by-area.md).

## Data Across Seams

What each stage receives and produces is fixed by the stage seam above; the
specific representations are out of scope here. The proof phase's residual
facts and the cache's stored headers are the two pieces of data whose shape
matters across seams, and both are covered by ADRs 0005, 0006, and 0015.

## Out Of Scope

Stage internals, IR node shapes, diagnostic codes, the tiny backend's
instruction set, and the language-server query surface. Each is a separate
session.
