# Zith

[![Build](https://github.com/GalaxyHaze/Zith-Lang/actions/workflows/ci.yml/badge.svg)](https://github.com/GalaxyHaze/Zith-Lang/actions)
[![License](https://img.shields.io/github/license/GalaxyHaze/Zith-Lang)](./license)
[![Version](https://img.shields.io/github/v/release/GalaxyHaze/Zith-Lang)](https://github.com/GalaxyHaze/Zith-Lang/releases)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white)](https://discord.gg/a7h4cpWHg4)

Zith is a systems language that lets you choose how you will implement a task. 
You can follow the idiomatic Zith-style, if you already have C experience, 
use C-style, functional & etc..., it only depends on which one you're more confortable with.
Start with the concepts you need, and learn more of the language as your work calls for it.

## Different Ways, Same output

Each example classifies a score into a bucket and prints the result. All three
print `bucket=1`.
the main idea here is: you can locally use different styles,
but everyone shall use the same API

### Idiomatic Zith

Uses `when` for classification and the standard library for output.

```zith
from std/io/console

fn classify(score: i32): i32 {
    return when (score) {
        (0..49) 0,
        (50..79) 1,
        (_) 2
    };
}

fn main() {
    let bucket = classify(73);
    println("bucket=#", bucket);
}
```

Runnable file: [`examples/styles-zith.zith`](examples/styles-zith.zith)

```bash
./build/zithc --include stdlib run examples/styles-zith.zith
```

### C-style

Uses C header import, conditional statements, and `printf`.

```zith
import "stdio.h"

fn classify(score: i32): i32 {
    if (score < 50) { return 0; }
    if (score < 80) { return 1; }
    return 2;
}

fn main(): i32 {
    let bucket = classify(73);
    _ = printf("bucket=%d\n", bucket);
    return 0;
}
```
> Use C-style to get habituated with the language, and gradually migrate to idiomatic Zith

Runnable file: [`examples/styles-c.zith`](examples/styles-c.zith)

```bash
./build/zithc --include stdlib run examples/styles-c.zith
```

### Functional-style

Uses `|>` to transform the score and `do` to print the result without changing
the value in the pipeline. This demonstrates that you can use a functional style,
but it doesn't mean Zith uses a complete functional programming model.

```zith
from std/io/console

fn classify(score: i32): i32 {
    when (score) {
        (0..49) 0,
        (50..79) 1,
        (_) 2
    };
}

fn main(): i32 {
    73  |> classify(..) do println("bucket=#", ..);
}
```

Runnable file: [`examples/styles-functional.zith`](examples/styles-functional.zith)

```bash
./build/zithc --include stdlib run examples/styles-functional.zith
```

> Zith highly depreceates to try to learn everything, you & your team shall pick a specific subset
and only work with it, and only try to learn a new feature, when the work calls for itsim 

The C-style example requires C header interop. In the current Zith-- compiler,
header bindings are provided through libclang. This is a current implementation
limitation, not a requirement of the full Zith design.

**Status: early development.** The available compiler implements **Zith--**, a
working subset of the full language. See the
[Zith-- specification](docs/Zith--.md) and the
[implementation status](docs/impl-status.md) for what works today.

---

## Quick Start

```bash
git clone https://github.com/GalaxyHaze/Zith-Lang.git
cd Zith-Lang
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

Run the standard-library example with:

```bash
./build/zithc --include stdlib run examples/test-import-console.zith
```

The compiler currently supports functions, structs, enums and unions, generics,
traits and interfaces, control flow, macros, and native code generation.
Zith-- includes a simplified part of the full language's ownership analysis.
The full Zith design also includes Node Resource Analysis (NRA) as one part of
its broader toolbox.

The full feature table, including planned and partial features, lives in
[docs/impl-status.md](docs/impl-status.md).

The `examples/` directory also contains runnable programs for the implemented
language surface:

```bash
./build/zithc check examples/optionals-simple.zith
./build/zithc run examples/state-defer-simple.zith
```

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
| `zithc docs` | Generate deterministic Markdown API docs from reachable source modules | Working |

`zithc docs` prints `API.md` to the terminal by default. Use `--spec` to include
all project symbols, `--out` to write under the project `docs/` directory,
`--out=PATH` to select a directory, and `--index --out=PATH` for a multipage
index. Existing generated files require `--force`. `--error` emits partial
documentation with an `Errors` section and still returns a failing exit code.

**Useful flags:** `--emit-cst`, `--emit-ast`, `--emit-hir`, `--emit-vir`, `--emit-ir`, `--emit-asm`, `-m release`, `--include <stdlib-path>`, `--cache-stats`

---

## Compilation Pipeline

```
Source -> Lex -> Parse/CST -> AST -> Scan -> Import -> Resolve -> TypeCheck -> Comptime/Solve -> NTA/NRA -> HIR -> VIR -> Codegen -> Cache
```

CST is emitted after parsing, AST after frontend lowering, and HIR after semantic
lowering. VIR is the optional lowering from HIR to the portable VM v2 IR. It is
distinct from LLVM IR, which is emitted by `--emit-ir`.

| Stage | Description |
|---|---|
| `Lex` | Tokenize source |
| `Parse/CST` | Build the concrete syntax tree from tokens |
| `AST` | Lower the parsed syntax tree into frontend declarations and expressions |
| `Scan` | Register top-level declarations |
| `Import` | Resolve module imports |
| `Resolve` | Bind names to symbols |
| `TypeCheck` | Infer and check types (sema) |
| `Comptime/Solve` | Generic instantiation and trait/conformance monomorphization run before NRA/HIR; macro expansion runs earlier in frontend |
| `NTA/NRA` | Residual ownership facts are accumulated before HIR; the full Zith ownership proof remains incomplete |
| `HIR` | Lower the typed, desugared program while attaching residual ownership facts |
| `VIR` | Optionally lower HIR to the portable VM v2 execution IR |
| `Codegen` | Emit LLVM IR -> native or WASM binary |

---

## Install

**Build from source** (recommended until stable release):

```bash
git clone https://github.com/GalaxyHaze/Zith-Lang.git
cd Zith-Lang
cmake -S . -B build
cmake --build build -j
```

**Release installer (Linux/macOS):**
```bash
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith-Lang/main/scripts/install.sh | bash
# Optional: install a specific version or the musl-linked Linux binary
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith-Lang/main/scripts/install.sh | bash -s -- v1.0.0
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith-Lang/main/scripts/install.sh | bash -s -- --musl
```

**Release installer (Windows PowerShell):**
```powershell
irm https://raw.githubusercontent.com/GalaxyHaze/Zith-Lang/main/scripts/install.ps1 | iex
```
Optional version pin:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/install.ps1 -Version v1.0.0
```

**Scoop (Windows):**
```powershell
scoop bucket add zithc https://github.com/GalaxyHaze/Zith-Lang.git
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
- [Issue Tracker](https://github.com/GalaxyHaze/Zith-Lang/issues)
- [Discussions](https://github.com/GalaxyHaze/Zith-Lang/discussions)

---

## License

[MIT License](./license)
