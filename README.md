# Zith

[![Build](https://github.com/GalaxyHaze/Zith/actions/workflows/ci.yml/badge.svg)](https://github.com/GalaxyHaze/Zith/actions)
[![License](https://img.shields.io/github/license/GalaxyHaze/Zith)](#license)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white)](https://discord.gg/a7h4cpWHg4)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.20%2B-red)](https://cmake.org/)

A systems language that lets you choose how you will implement a task.
Follow idiomatic Zith-style, C-style, functional, or whatever fits your team.

The current compiler implements **Zith--**, a working subset of the full language.
See the [Zith-- specification](docs/Zith--.md) and the
[implementation status](docs/impl-status.md) for what works today.

## Quick Start

```bash
git clone https://github.com/GalaxyHaze/Zith.git
cd Zith
cmake -S . -B build
cmake --build build -j
./build/zithc --help
```

Requires **CMake 3.20+**, a **C++23** compiler, and LLVM 18+ for native codegen.

## Documentation

| Document | Purpose |
|---|---|
| [Zith--.md](docs/Zith--.md) | Current compiler subset specification |
| [impl-status.md](docs/impl-status.md) | Verified status of every feature and command |
| [CHANGELOG.md](CHANGELOG.md) | What changed and what is planned |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Build instructions, code style, and pipeline overview |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for build instructions and code style.

- [Discord](https://discord.gg/a7h4cpWHg4) — progress updates and collaboration
- [Issue Tracker](https://github.com/GalaxyHaze/Zith/issues)
- [Discussions](https://github.com/GalaxyHaze/Zith/discussions)

## License

[MIT License](./license)
