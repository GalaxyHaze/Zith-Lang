# Zith

![Build](https://github.com/GalaxyHaze/Zith/actions/workflows/ci.yml/badge.svg)
![License](https://img.shields.io/github/license/GalaxyHaze/Zith)
![Version](https://img.shields.io/github/v/release/GalaxyHaze/Zith)

A statically typed, compiled, general-purpose language with compile-time ownership tracking and zero-cost abstractions.

**Status:** Active development — not production-ready.
You can follow us in [Discord](https://discord.gg/a7h4cpWHg4)
 if you want to colaborate or see the progress

---

## What Makes Zith Different

Zith uses **Node Resource Analysis (NRA)** — a compile-time pass that enforces ownership, borrowing, and memory safety without a borrow checker or garbage collector. No lifetime annotations, no runtime pauses, no hidden cost. The compiler proves your memory is safe before the program runs, and the syntax stays clean.

---

## Quick Start

```bash
zithc new hello-world
cd hello-world
zithc run
```

```zith
from std/io/console;

fn main() {
    @println("Hello, World!");
}
```

---

## Examples

### Ownership + Chain Flow

`lend` gives exclusive temporary mutation. `view` gives read-only access. The `->` operator pipes data left-to-right, and `!` propagates errors out of the chain automatically.

```zith
fn scale(p: lend Point, factor: f32) {
    p.x *= factor;
    p.y *= factor;
}

fn process(data: view Config) -> Response! {
    getData()
        -> parse(..)!
        -> scale(.., 2.0)
        -> save(..)!
}
```

### C Interop

Include a C header and all functions become available immediately. No bindings to write, no FFI layer to maintain.

```zith
import "openssl/ssl.h";

let ctx = SSL_CTX_new(method);   // all C functions available as raw fn
```

### Flow Functions

Structured goto with compile-time verification. Markers define jump targets, docks invoke them — the compiler ensures correctness.

```zith
flow fn run(data: Stream): void {
    marker Process(chunk: Chunk, count: i32) {
        transform(chunk);
    }

    for ( i = 0, item in data ) {
        dock { jump Process(item, i); }
        i += 1;
    }
}
```

---

## Language Highlights

### Memory & Ownership

- **NRA** — compile-time ownership tracking without a borrow checker
- `unique`, `share`, `view`, `lend`, `belong` — zero-cost memory modifiers

### Type System

- `struct`, `component` (POD), `enum` (C-style, struct-backed, ADT), `union` (tagged)
- Traits (nominal) vs interfaces (structural)
- Capabilities — `Copy`, `Arithmetic`, `Error`, `Share`, `Trust`, and more

### Control Flow

- `when` — pattern matching with ranges, types, and destructuring
- `flow fn` / `marker` / `dock` / `jump` — structured goto with compile-time verification
- `->` — chain flow operator for left-to-right pipelines

### Error Handling

- `?T` (optional) / `T!` (result) — zero-cost, return-based
- `or` — fallback chains: `!load() or backup() or default`
- `fail` blocks — scoped error recovery

### Compile-Time

- `comptime` — compile-time bindings and `const` blocks
- `const fn` — functions forced to resolve at compile time
- Reflection via `@` intrinsics

### Extensibility

- Words — custom operators (prefix, infix, suffix)
- Contexts — scoped DSL injection
- Macros — hygienic, raw, and tag macros

### Interop & Platform

- C interop — automatic `.h` binding, manual semantic annotation
- ZIR — portable intermediate format
- Multi-target — VM execution or native compilation via LLVM

---

## Install

### Package Managers

**Scoop:**
```powershell
scoop bucket add zithc https://github.com/GalaxyHaze/Zith.git
scoop install zithc
```

**Homebrew:**
```bash
brew tap galaxyhaze/zithc
brew install zithc
```

### Install Scripts

**Linux / macOS:**
```bash
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.sh | bash
```
Pass `--musl` for a statically linked musl build, or a version tag (e.g. `v0.0.1`).

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install.ps1 | iex
```

**WebAssembly:**
```bash
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/main/scripts/install-wasm.sh | bash
```

### Build from Source

```bash
git clone https://github.com/GalaxyHaze/Zith.git
cd Zith
cmake -S . -B build
cmake --build build -j
```

Requires CMake 3.15+, a C++17 compiler (GCC/Clang/MSVC), and optionally LLVM for native backend support.

**Verify:**
```bash
zithc --help
```

---

## CLI Commands

| Command | Description |
|---------|-------------|
| `zithc build` | Build the project |
| `zithc build -m release` | Release build |
| `zithc check` | Syntax and type checking without compilation |
| `zithc execute <file.zbc>` | Run a compiled bytecode file |
| `zithc run` | Build and run in one command |

---

## Documentation

- [Language Specification](docs/Zith-spec.md) — overview, quick reference, and appendix
- [Full docs](https://zith-lang.org)

### Spec Sections

| # | Topic | File |
|---|-------|------|
| 2 | [Module System](docs/02-module-system.md) | `import`, `from`, `export`, visibility |
| 3 | [Type System](docs/03-type-system.md) | Primitives, structs, enums, unions, generics |
| 4 | [Traits & Interfaces](docs/04-traits-interfaces.md) | Nominal traits, structural interfaces, capabilities |
| 5 | [Functions](docs/05-functions.md) | `fn`, `const fn`, `async fn`, `flow fn`, `raw fn` |
| 6 | [Mutability & Bindings](docs/06-mutability-bindings.md) | `let`, `var`, `global`, `const`, deep mutability |
| 7 | [Memory Model (NRA)](docs/07-memory-model.md) | Ownership, modifiers, the four rules |
| 8 | [Error Handling](docs/08-error-handling.md) | `?T`, `T!`, `with`/`catch`, `fail` blocks |
| 9 | [Control Flow](docs/09-control-flow.md) | `if`, `when`, `for`, `->`, markers, docks |
| 10 | [Concurrency](docs/10-concurrency.md) | `spawn`, `await`, thread safety |
| 11 | [Comptime](docs/11-comptime.md) | `const`, reflection, intrinsics |
| 12 | [Assets](docs/12-assets.md) | Compile-time asset processing |
| 13 | [Raw & Unsafe](docs/13-raw-unsafe.md) | `raw`, `unsafe`, `Trust` |
| 14 | [Polymorphism](docs/14-polymorphism.md) | `dyn`, static vs dynamic dispatch |
| 15 | [Macros](docs/15-macros.md) | Scoped, raw, tag macros |
| 16 | [Words](docs/16-words.md) | Custom operators |
| 17 | [Contexts](docs/17-contexts.md) | DSL bundling, scoped activation |
| 18 | [C Interop](docs/18-c-interop.md) | `.h` import, `extern 'C'` |
| 19 | [Project Config](docs/19-project-config.md) | `ZithProject.toml`, `ZithFlags` |
| 20 | [Standard Library](docs/20-standard-library.md) | `std`, `soon`, `c` namespaces |
| 21 | [Best Practices](docs/21-best-practices.md) | Ownership patterns, naming conventions |

---

## Contributing

- [Issue Tracker](https://github.com/GalaxyHaze/Zith/issues)
- [Discussions](https://github.com/GalaxyHaze/Zith-discussions/discussions)

See the [Build from Source](#build-from-source) section to get a local development environment set up.

---

## License

[MIT License](./license)
