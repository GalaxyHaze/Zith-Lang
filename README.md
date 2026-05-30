# Zith Programming Language

The `rewrite` branch is the future main branch of Zith programming language. The current `master` was a prototype. While `rewrite` doesn't catch up to `master`, `rewrite` is an early WIP of the future version of the main version — a ground-up rebuild with a cleaner architecture, modern C++23, and a compiler pipeline designed for extensibility.

## Quick Start

```bash
cmake -B build
cmake --build build
./zith <file>
```

## Goals

- Simple, readable syntax with strong static typing
- First-class compile-time evaluation
- Low-level control without sacrificing safety
- Fast compilation with incremental support

## License

MIT
