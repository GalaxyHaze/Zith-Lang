# Contributing to Zith (Procedural)

This branch is a ground-up rebuild of the Zith compiler.

## Guidelines

- **C++23** with no exceptions, no RTTI
- Arena-backed allocation for all compiler data
- `constexpr`-first: frozen containers for compile-time tables
- No raw pointers where arena IDs work
- Code must compile with `-Wall -Werror -Wpedantic -Wextra`

## Structure

```
src/memory/   — Arena, DynArray, Optional, Result, FlatMap, StringInterner
src/common/   — Shared utilities (format, overloaded)
src/support/  — Platform abstractions
```

## Build

```bash
cmake -B build
cmake --build build
```
