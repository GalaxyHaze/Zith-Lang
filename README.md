# Zith

A statically-typed systems programming language with compile-time ownership tracking and zero-cost abstractions.

**Status:** Active development — not production-ready.

---

## Install

### Package Managers

**Scoop:**
```powershell
scoop bucket add zith https://github.com/GalaxyHaze/Zith.git
scoop install zith
```

**Homebrew:**
```bash
brew tap galaxyhaze/zith
brew install zith
```

### Install Scripts

**Linux / macOS:**
```bash
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/master/scripts/install.sh | bash
```
Pass `--musl` for a statically linked musl build, or a version tag (e.g. `v0.0.1`).

**Windows (PowerShell):**
```powershell
irm https://raw.githubusercontent.com/GalaxyHaze/Zith/master/scripts/install.ps1 | iex
```

**WebAssembly:**
```bash
curl -fsSL https://raw.githubusercontent.com/GalaxyHaze/Zith/master/scripts/install-wasm.sh | bash
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
zith --help
```

---

## Quick Start

```bash
zith new hello-world
cd hello-world
zith run
```

```zith
from std/io/console;

fn main() {
    println("Hello, World!");
}
```

---

## CLI Commands

| Command | Description |
|---------|-------------|
| `zith build` | Build the project |
| `zith build -m release` | Release build |
| `zith check` | Syntax and type checking without compilation |
| `zith compile <file>` | Compile to ZBC bytecode |
| `zith execute <file.zbc>` | Run a compiled bytecode file |
| `zith run` | Build and run in one command |

---

## Language Highlights

- **Ownership system** — `unique`, `share`, `view`, `lend`, `extension` keywords enforced at compile time
- **No garbage collector** — memory safety without runtime overhead
- **ZBC bytecode** — portable, versioned intermediate format
- **Multi-execution** — run via VM or compile to native via LLVM
- **Contexts & DSLs** — scoped operator/constant injection
- **Structured goto** — `marker`, `entry`, `exit` for verified control flow

Full language specification: `Zith-spec.md`

---

## Documentation

- [Full docs](https://galaxyhaze.github.io/Zith/)
- [Architecture & status matrix](docs-architecture.md)
- [API headers](include/zith/README.md)

---

## Resources

- [Issue Tracker](https://github.com/GalaxyHaze/Zith/issues)
- [Discussions](https://github.com/GalaxyHaze/Zith/discussions)
- [Repository](https://github.com/GalaxyHaze/Zith)

---

## License

[MIT License](./license)