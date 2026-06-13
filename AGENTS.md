# AGENTS.md — Zith compiler (zithc)

## Build & Test

```bash
cmake -B build -DBUILD_TESTING=ON     # configure (CMake 3.20+, C++23, Clang 18+/GCC 14+)
cmake --build build -j                 # build compiler + tests
ctest --test-dir build                 # run all tests
./build/zithc-lexer-test               # single test binary
cmake --build build --target fmt       # format (clang-format, LLVM style)
cmake --build build --target fmt-check # dry-run format check
```

Tests use a custom framework (`CHECK`/`CHECK_EQ` macros + `TEST_MAIN` in `tests/test-common.hpp`). No external test runner — each test is a standalone binary.

## Architecture

- **Entrypoint:** `src/cli/zithc-main.cpp` → `zith::cli::CompilerDriver::run(opts)`
- **Pipeline:** `lexer/` → `parser/` → `sema/` → `zir/hir/` → `zir/mir/`
- **Support:** `ast/`, `diagnostics/`, `import/`, `memory/`, `types/`, `cli/`
- **stdlib:** `stdlib/` (`.zith` files), compiled into binary via `ZITH_STDLIB_DIR`
- **Dependencies:** `mio` (mmap), `tomlplusplus` — fetched via CMake `FetchContent`, no system install needed

## Conventions

- **Namespace:** `zith::<layer>::<module>` (e.g. `zith::lexer`, `zith::cli::commands`)
- **Names:** `snake_case` files, `PascalCase` types, `camelCase` functions
- **Headers:** `#pragma once`, no include guards
- **Memory:** Arena allocation (`zith::memory::Arena`) preferred over `new`/`delete`
- **Style:** LLVM-derived (4-space indent, 100 cols, Attach braces, Right pointer alignment) — see `.clang-format`
- **Flags:** Defaults in `ZithFlags.toml`, overridden by `ZithProject.toml` (searched CWD-upward), CLI flags win

## Key CLI Commands

`zithc build`, `check`, `compile <file>`, `execute <file.zbc>`, `run`, `test`, `fmt`, `docs`, `new`, `clean`, `deps`, `repl`

## CI / Release

- Branch `rewrite` is main development branch
- Release builds cross-compile for 6 targets (Linux amd64/arm64, macOS universal, Windows amd64/arm64, Linux musl, WASM)
- `BUILD_TESTING=OFF` and `ENABLE_WERROR=OFF` in CI release builds
- Use `ZITH_VERSION` CMake variable to set version, falls back to latest git tag or `v0.0.1`
- WASM builds use Emscripten (`emcmake cmake`), set `ZITH_IS_WASM=ON`
