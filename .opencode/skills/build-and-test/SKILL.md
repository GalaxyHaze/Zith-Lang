---
name: build-and-test
description: >-
  Use when discussing building, testing, compiling, running, formatting, or
  debugging the Zith compiler (zithc). Covers CMake commands, test binaries,
  CTest, clang-format, and CI build matrix specifics.
---

# Build & Test

## Build

```bash
cmake -B build -DBUILD_TESTING=ON              # configure (C++23, CMake 3.20+, Clang 18+/GCC 14+)
cmake --build build -j                           # build compiler + test binaries
./build/zithc                                    # run the compiler
```

## Test

```bash
ctest --test-dir build                           # run all registered tests
./build/zithc-lexer-test                         # single test binary
```

Test binaries: `zithc-lexer-test`, `zithc-parser-expr`, `zithc-parser-stmt`, `zithc-parser-scan`, `zithc-parser-full`, `zithc-sema-test`, `zithc-type-dedup`, `zithc-unify-errors`, `zithc-sema-hir`, `zithc-mir-test`, `zithc-import-test`, `zithc-import-e2e`.

Test framework: custom `CHECK`/`CHECK_EQ` macros + `TEST_MAIN` in `tests/test-common.hpp`. Each test binary is a standalone executable, no external framework.

## Format

```bash
cmake --build build --target fmt                 # format in-place (clang-format)
cmake --build build --target fmt-check           # dry-run, errors on diff
```

Style: LLVM-derived, 4-space indent, 100 cols, Attach braces, Right pointer alignment (see `.clang-format`).

## Dependencies

Both fetched by CMake `FetchContent` — no system install:
- `mio` (memory-mapped IO)
- `tomlplusplus` (TOML parsing)

## CI Build Matrix

Release builds cross-compile for 6 targets via `build-artifact.yml`:
- `linux-amd64` (Clang)
- `linux-arm64` (Clang, cross)
- `macos-universal` (Clang via Homebrew, x86_64+arm64)
- `windows-amd64` (clang-cl)
- `windows-arm64` (clang-cl, cross)
- `linux-amd64-musl` / `linux-arm64-musl` (Zig cc as cross-compiler)
- `wasm` (Emscripten, `emcmake cmake`, `ZITH_IS_WASM=ON`)

Release builds use `-DBUILD_TESTING=OFF -DENABLE_WERROR=OFF`.

## Config Loading

Defaults from `ZithFlags.toml`. Override via `ZithProject.toml` (searched CWD-upward). CLI flags always win.
