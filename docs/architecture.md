# Self-Zith Architecture (General API)

This is the general shape of the Self-Zith compiler: the modules, the seams
between them, and the data that crosses each seam. It is deliberately not a
spec and carries no stage internals. Each stage gets its own session and its
own document later.

Vocabulary is in [`CONTEXT.md`](../CONTEXT.md). The decisions behind this shape
are ADRs [0001](adr/0001-compiler-core-exposes-one-narrow-seam.md) through
[0013](adr/0013-c-toolchain-is-an-adapter.md). The cross-compile strategy is in
[the cross-compile plan](plans/cross-compile.md).

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
  cache                     transversal; materialized once at the end
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

**IR**: HIR, the last front-side representation and the single backend entry
point.

**Backends**: VIR and its consumers (VM now, tiny later), and the reused LLVM
emitters reading HIR directly.

**Drivers**: the CLI driver and the language-server driver. They own the
persistent state the core does not. A WASM playground driver is a third driver
over the same seam.

**C interop**: the C binder and the C ABI surface, a module behind the external
seam used by the frontend and the backends. The C object emitter (the tiny
backend) is a backend, not part of this module.

**OS-C layer**: the OS-sensitive operations (file system, process control,
time, memory mapping, terminal) isolated in C behind one narrow interface, so
the core stays target-agnostic. Porting to a new OS writes one adapter here; see
[ADR-0012](adr/0012-os-sensitive-code-is-a-c-layer.md).

**C toolchain**: an adapter behind a seam with a `ToolchainBackend` contract
(native compile, link, header import, sysroot discovery). Adapters are tried in
a deterministic order: clang + `--target` + `lld` first, Zig second, native
cross-GCC third; see [ADR-0013](adr/0013-c-toolchain-is-an-adapter.md).

**Cache**: not a module in the call chain; a transversal concern materialized
once at the end by the driver.

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

The core and the Zith sources are target-agnostic; portability is confined to
the OS-C layer and the C toolchain adapter (ADR-0012, ADR-0013).

| Target | How it is produced |
|---|---|
| Linux amd64/arm64 (glibc/musl) | clang + `--target` + `lld` with a sysroot, or Zig |
| WASM/WASI | `wasi-sdk` or Zig's wasm targets |
| Windows (GNU ABI) | Zig or llvm-mingw |
| Windows (MSVC ABI) | `xwin` + clang-cl + `lld-link` locally, or a Windows runner |
| macOS | build on a Mac yourself; the Apple SDK is not redistributable (see [macOS build policy](notes/macos-build-policy.md)) |

macOS and Windows-MSVC have no freely redistributable sysroot. Windows-MSVC can
still be built locally on Linux with `xwin` (which fetches the Microsoft CRT and
SDK headers under Microsoft's own terms) plus `clang-cl` and `lld-link`; macOS
cannot, so we do not ship macOS binaries and ask users to build on a Mac
themselves (see [macOS build policy](notes/macos-build-policy.md)).

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
matters across seams, and both are covered by ADRs 0005 and 0006.

## Out Of Scope

Stage internals, IR node shapes, diagnostic codes, the tiny backend's
instruction set, and the language-server query surface. Each is a separate
session.
