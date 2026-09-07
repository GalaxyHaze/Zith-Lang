# Zith

[![Build](https://github.com/GalaxyHaze/Zith/actions/workflows/ci.yml/badge.svg)](https://github.com/GalaxyHaze/Zith/actions)
[![License](https://img.shields.io/github/license/GalaxyHaze/Zith)](./license)
[![Version](https://img.shields.io/github/v/release/GalaxyHaze/Zith)](https://github.com/GalaxyHaze/Zith/releases)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white)](https://discord.gg/a7h4cpWHg4)

> **Status: early development.** The compiler is a working Zith-- subset: lexing/parsing,
> type checking, generics, HIR, LLVM codegen, a growing stdlib, and an executing CLI pipeline are
> implemented. Some spec-level language features are still unsupported or partial. See
> [docs/impl-status.md](docs/impl-status.md) for the verified status of every feature.

A statically typed, compiled, system language. 'Zith' proves memory safety at compile time
through **Node Resource Analysis (NRA)** no garbage collector, no traditional borrow checker or annotation, no runtime
overhead. The syntax stays clean and the compiler does the hard work.

The current `main` compiles **Zith--**, a simplified subset documented in
[`docs/Zith--.md`](docs/Zith--.md). The subset keeps the existing type system and, instead of
**comptime**, provides normal/raw **macros** (Zith-- only). Binding semantics, pointer/borrow
rules, generics, `dyn`, enum/union templates, `defer`, and other supported behavior are
documented in
[`docs/Zith---implementation.md`](docs/Zith---implementation.md).

---

## What Makes Zith Different

'Zith' is designed around memory safety without lifetime annotations or a garbage collector. The
current main implements a simplified ownership core: `lend`/`view` borrow slices, logical moves
from `&x`, pointer-escape checks, and NRA residual facts before HIR; the full NRA is planned for
future Zith. Zith aims to be an expressive and clear language, with specialized tools for
domain-specific work. **Zith--** already demonstrates that direction with generics,
traits/interfaces, `dyn` dispatch, `state` machines and rich control flow.

---

## Quick Start

```bash
git clone https://github.com/GalaxyHaze/Zith.git
cd Zith
cmake -S . -B build
cmake --build build -j
./build/zithc --help
```

Requires **CMake 3.20+**, a **C++23** compiler, and LLVM 18+ for the codegen backend. LLVM is
optional: without it, `check` and HIR emission still work, but native codegen is disabled.

**Hello, World** with the C stdio interop surface:

```zith
import "stdio.h"

fn main(){
    printf("Hello, World!");
}
```

The canonical stdlib console API:

```zith
from std/io/console

fn main(){
    println("Hello, World!");
}
```

Run either with:

```bash
./build/zithc run examples/test-import-console.zith
```

The suites under `examples/` cover the working Zith-- surface, including bindings, generics,
optionals, macros, ownership, dyn interfaces, loops, `when`, `state`/`defer`, variadic slices,
and C interop:

```bash
./build/zithc check examples/optionals-simple.zith
./build/zithc run examples/state-defer-simple.zith
```

---

## Language Overview

**Type System**

- Primitives: `u8`-`u128`, `i8`-`i128`, `f32`, `f64`, `bool`, `char`, `void`
- Composite: `struct`, `component` (POD), `enum`/`union` (including generic enum/union templates)
- Generics, nominal `type`, transparent `alias`, and pattern matching with `when`

**Memory Model (NRA)**

- `lend` — exclusive mutable borrow for the call
- `view` — read-only borrow
- Logical move semantics for `&x` and for `self`/`var self` method calls
- Pointer-escape checks for address-of and `@ptrOf(local)`
- NRA residual facts before HIR; the full NRA proof is planned for Zith

**Functions**

- `fn` — regular function
//a bit useless rigth now, since everything is a gray state
- `raw fn` — opt out of NRA for C-interop
- `extern fn` — fixed C ABI linkage
- Generic functions with explicit or inferred type arguments
- Function values `fn(...): R`
- `state` machines with `dock` / `jump` (direct `tailcc` transitions)
- `const fn` — planned for Zith; compile-time evaluation is not implemented

**Control Flow**

- `if` / `else` / `else (cond)`
- `when` for pattern matching, including guards, ranges, and narrowing
- `for` in conditional, infinite, 3-clause, and iterator form
- `break` / `continue` with labels, and `defer` scope guards
- `->` pointer arrow access (`p->field`)

**Error Handling**

- `?T` optional values with implicit condition tests and `?` propagation where valid (Zith-- only)
- `is null`, `must`, and `raw` optional extraction (Zith-- only)
- `fail` / `with` / `catch` / `throw` are planned for Zith and not implemented yet

**Extensibility and C Interop**

- `macro` / `raw macro` declarations and `@name(...)` calls (Zith-- only)
- Validated C header imports through libclang
- `tag` (formerly `tag macro`), `word`, `context`, and `use` are planned for Zith

---

## Implementation Status

| Feature | Status | Notes |
|---|---|---|
| Lexer / parser | **Working** | Hand-written lexer and recursive-descent parser |
| Formatter (`zithc fmt`) | **Working** | Round-trip stable across the expression AST |
| Type checking and name resolution | **Working** | Imports, generics, traits/interfaces, all expression nodes |
| Generic instantiation | **Working** | Generic functions, structs, aliases, enum/union templates, implement blocks |
| HIR lowering | **Working** | Covers the working Zith-- feature set |
| LLVM codegen | **Working** | x86-64 and WebAssembly targets |
| `fn`, `raw fn`, `extern fn` | **Working** | `extern fn` is C-ABI-only |
| `state`, `dock`, `jump` | **Working** | Direct `tailcc` transitions |
| `struct`, `enum`, `union`, `component` | **Working** | Includes generic enum/union templates with methods and conformance |
| `trait`, `interface`, `implement` | **Working** | Nominal and structural conformances; `dyn` method dispatch |
| Primitive arithmetic and comparisons | **Working** | Includes bitwise operations, ranges, `in`, and compound assignment |
| `when` / `match` / `for` / labels / `defer` | **Working** | |
| Module imports (`import`, `from`, `export`) | **Working** | Includes platform-specific imports and visibility controls |
| `alias`, `type` | **Working / Partial** | `alias` works; nominal `type` needs explicit value construction/access syntax |
| C interop | **Working** | Manual `extern fn` plus validated C header imports through libclang |
| `macro` / `raw macro` / `@name(...)` | **Working (Zith-- only)** | Normal and raw macros with call-site scope handling |
| Field access, index, deref, address-of | **Working** | Optional bounds checks on array/slice indexing; `raw` skips checks |
| `?T` (Zith-- only) | **Working** | Optional values, `?` propagation where valid, `is null`, `must`, and `raw` extraction |
| `is` / `as` | **Working** | Casting for numeric pairs and raw pointers; tagged-union/opaque narrowing |
| `tag` (formerly `tag macro`) | Planned for Zith | Rejected in Zith--; planned under the shorter `tag` name |
| `word` / `context` / `use` | Planned for Zith | No working semantics in Zith-- |
| `const fn`, `comptime` | Planned for Zith | Compile-time evaluation is not implemented in Zith-- |
| Full NRA proof | Planned for Zith | Zith-- currently implements `lend`/`view` slices, logical moves, and escape checks |
| Core concurrency syntax (`async fn`, `yield`, `spawn`, `await`) | Not part of the core language contract | Concurrency is documented as stdlib/runtime APIs instead of frontend syntax |
The single source of truth with per-feature verification notes is
[docs/impl-status.md](docs/impl-status.md). Feature IDs, waves, and current roadmap details are in
[docs/roadmap.md](docs/roadmap.md).

---

## WebAssembly Playground

The browser playground is a compiler-in-the-browser tool, not a program runner. It exports the
stable `zith_compile_source` / `zith_run_source` contract for lexing, type checking, diagnostics,
and textual emission up to HIR. `zith_run_source` performs check plus HIR output and does not
execute the submitted program in the browser. Native execution is available through `zithc run` on
the host.

The ABI, return codes, `mode`, `emit_mask`, and buffer accessors are documented in
[docs/wasm-playground-abi.md](docs/wasm-playground-abi.md).

---

## CLI Reference

| Command | Description | Status |
|---|---|---|
| `zithc build` | Compile to native binary | Working |
| `zithc run` | Compile and execute | Working |
| `zithc check` | Type-check without emitting | Working |
| `zithc fmt` | Format source files | Working |
| `zithc create <name>` | Scaffold a new project | Working |
| `zithc clean` | Remove build artifacts | Working |
| `zithc execute <file>` | Run a pre-compiled binary | Working |
| `zithc test <path>` | Discover and run test files under a path | Working |
| `zithc repl` | Interactive REPL | 'Zith' only |
| `zithc deps list` | List declared dependencies | Working |
| `zithc deps add` / `deps remove` | Dependency management | Stub |
| `zithc docs` | Generate documentation from source | Working |

**Useful flags:** `--emit-ast`, `--emit-hir`, `--emit-ir`, `--emit-asm`, `-m release`, `--include <stdlib-path>`, `--cache-stats`

---

## Compilation Pipeline

```
Source -> Lex -> Scan -> Import -> Resolve -> TypeCheck -> Comptime/Solve -> NTA/NRA -> HIR -> Codegen -> Cache
```

| Stage | Description |
|---|---|
| `Lex` | Tokenize source |
| `Scan` | Register top-level declarations |
| `Import` | Resolve module imports |
| `Resolve` | Bind names to symbols |
| `TypeCheck` | Infer and check types (sema) |
| `Comptime/Solve` | Reserved for generic instantiation and the solved semantic view (planned for 0.7.0 step-04); macro expansion runs earlier in frontend |
| `NTA/NRA` | Fact accumulation plus Node Resource Analysis — the target pre-HIR ownership phase (still a stub) |
| `HIR` | Lower the typed, desugared program while attaching residual ownership facts |
| `Codegen` | Emit LLVM IR -> native or WASM binary |

---

## Install

**Build from source** (recommended until stable release):

```bash
git clone https://github.com/GalaxyHaze/Zith.git
cd Zith
cmake -S . -B build
cmake --build build -j
```

**Release installer (Linux/macOS):**
```bash
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.sh | bash
# Optional: install a specific version or the musl-linked Linux binary
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.sh | bash -s -- v1.0.0
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.sh | bash -s -- --musl
```

**Release installer (Windows PowerShell):**
```powershell
irm https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.ps1 | iex
```
Optional version pin:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/install.ps1 -Version v1.0.0
```

**Scoop (Windows):**
```powershell
scoop bucket add zithc https://github.com/GalaxyHaze/Zith.git
scoop install zithc
```

## Lexer Benchmark

The local lexer benchmark is opt-in and has no external benchmark dependency:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DZITH_BUILD_BENCHMARKS=ON
cmake --build build-release --target bench-lexer -j
./build-release/bench-lexer
./build-release/bench-lexer --format json
```

`bench-lexer` measures `lexer::tokenize` over the synthetic `mixed-valid` scenario. Source-map
file registration and host-information collection are outside the timed region; each tokenization
includes a new temporary arena, diagnostics state, and token stream allocation. Text and JSON
output identify the operating system, CPU model, logical CPU count, total memory, and compiler.
Compare results only on the same machine, with the same compiler and build flags. The JSON output
is intended for local automation and is not a versioned public API.

## ASCII Highlighting

The standalone script `scripts/zith-ascii.py` renders Zith source with visible highlighting when
no plugin highlighter is available. It accepts a file, stdin, or an inline `--string`, and supports
`markdown`, `prefix`, `compact`, `ansi`, `tag`, and `plain` styles. `ansi` wraps the output in a
` ```ansi ` block so it renders when pasted into Discord. The `ansi` palette uses only Discord-safe
ANSI colors (`30`-`37`). `tag` prints the same code content as plain `[0;35m...`-style markers for
non-rendering displays.

```bash
scripts/zith-ascii.py examples/bindings-simple.zith
scripts/zith-ascii.py --style tag --string 'fn main() { printf("oi"); }'
```

**Homebrew (macOS/Linux):**
```bash
brew tap galaxyhaze/zithc
brew install zithc
```

---

## Documentation

| Document | Purpose |
|---|---|
| [Language Spec](docs/Zith-spec.md) | Overview, design goals, quick reference, appendix |
| [Full Spec](docs/Zith-spec-full.md) | All chapters in one file |
| [Implementation Status](docs/impl-status.md) | Verified status of every feature stage and CLI command |
| [Roadmap](docs/roadmap.md) | Feature IDs, dependency graph, and implementation waves |
| [Zith--](docs/Zith--.md) | Current compiler subset specification |
| [CHANGELOG](CHANGELOG.md) | What changed and what is planned |
| [CONTRIBUTING](CONTRIBUTING.md) | Build instructions, code style, pipeline overview |

### Spec Chapters

| # | Topic | File |
|---|---|---|
| 1 | Overview & Design Philosophy | [Zith-spec.md](docs/Zith-spec.md) |
| 2 | Module System | [02-module-system.md](docs/02-module-system.md) |
| 3 | Type System | [03-type-system.md](docs/03-type-system.md) |
| 4 | Traits, Interfaces & Capabilities | [04-traits-interfaces.md](docs/04-traits-interfaces.md) |
| 5 | Functions | [05-functions.md](docs/05-functions.md) |
| 6 | Mutability & Bindings | [06-mutability-bindings.md](docs/06-mutability-bindings.md) |
| 7 | Memory Model (NRA) | [07-memory-model.md](docs/07-memory-model.md) |
| 8 | Error Handling | [08-error-handling.md](docs/08-error-handling.md) |
| 9 | Control Flow | [09-control-flow.md](docs/09-control-flow.md) |
| 10 | Concurrency | [10-concurrency.md](docs/10-concurrency.md) |
| 11 | Comptime | [11-comptime.md](docs/11-comptime.md) |
| 12 | Assets | [12-assets.md](docs/12-assets.md) |
| 13 | Raw & Unsafe | [13-raw-unsafe.md](docs/13-raw-unsafe.md) |
| 14 | Polymorphism | [14-polymorphism.md](docs/14-polymorphism.md) |
| 15 | Macros | [15-macros.md](docs/15-macros.md) |
| 16 | Words (Custom Operators) | [16-words.md](docs/16-words.md) |
| 17 | Contexts | [17-contexts.md](docs/17-contexts.md) |
| 18 | C Interop | [18-c-interop.md](docs/18-c-interop.md) |
| 19 | Project Configuration | [19-project-config.md](docs/19-project-config.md) |
| 20 | Standard Library | [20-standard-library.md](docs/20-standard-library.md) |
| 21 | Best Practices | [21-best-practices.md](docs/21-best-practices.md) |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions and code style.

- [Discord](https://discord.gg/a7h4cpWHg4) — progress updates and collaboration
- [Issue Tracker](https://github.com/GalaxyHaze/Zith/issues)
- [Discussions](https://github.com/GalaxyHaze/Zith/discussions)

---

## License

[MIT License](./license)
