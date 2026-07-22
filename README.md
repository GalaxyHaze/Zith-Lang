# Zith — Procedural

Experimental branch for rethinking the Zith compiler pipeline.

This branch starts from zero with only the core infrastructure:
arena allocation, dynamic arrays, optional/result types, and
frozen constexpr containers.

The goal is to rebuild the compiler with:
- Constexpr-driven lexer (perfect hash tables for keywords + operators)
- Unified scan/sema parser pass
- Section-based binary cache format
- Complete complex type system (optional, result, unions, dyn)

## Build

```bash
cmake -B build
cmake --build build
```
